#include "core.h"
#include <lucas/storage/sd/Card.h>
#include <utility>
#include <lucas/lucas.h>
#include <lucas/Spout.h>
#include <lucas/Boiler.h>
#include <lucas/Station.h>
#include <lucas/MotionController.h>
#include <lucas/RecipeQueue.h>
#include <lucas/info/info.h>
#include <lucas/serial/serial.h>

#include <lucas/serial/FirmwareUpdateHook.h>
#include <src/module/planner.h>

namespace lucas::core {
static auto s_calibration_phase = CalibrationPhase::None;
static std::optional<s32> s_scheduled_calibration_temperature = std::nullopt;
static util::Timer s_time_since_setup = {};

static f32 s_startup_temperature = 0.f;
static std::optional<s32> s_last_session_target_temperature = std::nullopt;

void setup() {
    MotionController::the().setup();

    if (CFG(MaintenanceMode)) {
        Spout::the().setup_pins();
        Boiler::the().setup_pins();
        Station::setup_pins(5);
        return;
    }

    Boiler::the().setup();
    Spout::the().setup();
    RecipeQueue::the().setup();
    Station::setup();

    MotionController::the().home();
    MotionController::the().travel_to_sewer();

    s_startup_temperature = Boiler::the().temperature();
    s_last_session_target_temperature = Boiler::the().stored_target_temperature();
}

void tick() {
    if (CFG(MaintenanceMode)) {
        // for button press logs
        Station::tick();

        if (Boiler::the().target_temperature())
            Boiler::the().tick();

        return;
    }

    // boiler
    if (not is_filtered(Filter::Boiler))
        Boiler::the().tick();

    // queue
    if (not is_filtered(Filter::RecipeQueue))
        RecipeQueue::the().tick();

    RecipeQueue::the().remove_finalized_recipes();

    // stations
    if (not is_filtered(Filter::Station))
        Station::tick();

    Station::update_leds();

    // spout
    if (not is_filtered(Filter::Spout))
        Spout::the().tick();

    if (not s_time_since_setup.is_active()) {
        inform_calibration_status();
        s_time_since_setup.start();
        RecipeQueue::the().reset_inactivity();
    }

    if (s_time_since_setup >= 30s and s_calibration_phase == CalibrationPhase::None) {
        LOG("calibracao automatica iniciada");
        Station::initialize(std::nullopt, std::nullopt);
        calibrate(std::nullopt);
        return;
    }
}

void calibrate(std::optional<s32> target_temperature) {
    if (target_temperature == Boiler::the().target_temperature())
        return;

    auto& boiler = Boiler::the();
    auto& spout = Spout::the();
    auto& flow_controller = Spout::FlowController::the();

    // maybe we are already calibrating...
    switch (s_calibration_phase) {
    // if we're still just reaching the target temp just update the target temperature and keep waiting
    case CalibrationPhase::ReachingTargetTemperature:
        LOG_IF(LogCalibration, "trocando temperatura target");
        boiler.update_target_temperature(target_temperature);
        return;
    // if we're doing flow analysis it gets a bit more complex
    // we tell the flow controller it should abort, wait for it's loop to be called again and save the new desired temperature for later
    // "later" in this case is a few lines below, where `s_scheduled_calibration_temperature` is used.
    // the flow of the code gets really nasty in the current architecture, need to rewrite everything.
    case CalibrationPhase::AnalysingFlowData:
        LOG_IF(LogCalibration, "cancelando analise de fluxo");
        flow_controller.set_abort_analysis(true);
        s_scheduled_calibration_temperature = target_temperature;
        return;
    default:
        break;
    }

    LOG_IF(LogCalibration, "iniciando nivelamento");

    {
        info::TemporaryCommandHook hook{ info::Command::RequestInfoCalibration, &Boiler::inform_temperature_status };
        s_calibration_phase = CalibrationPhase::ReachingTargetTemperature;

        boiler.update_and_reach_target_temperature(target_temperature);
    }

    const auto restarted_not_long_ago = s_startup_temperature >= 60.f; // water is still warm
    const auto same_target_as_last_analysis = flow_controller.last_analysis_target_temperature() == boiler.target_temperature();

    const auto should_reuse_flow_analysis_data = restarted_not_long_ago and same_target_as_last_analysis;
    if (should_reuse_flow_analysis_data and not CFG(ForceFlowAnalysis)) {
        LOG_IF(LogCalibration, "reutilizando dados de analise de fluxo");
        flow_controller.fetch_digital_signal_table_from_file();

        // force the flow controller to inform the host that analysis is finished
        flow_controller.update_status(Spout::FlowController::FlowAnalysisStatus::Done);
    } else {
        info::TemporaryCommandHook hook{ info::Command::RequestInfoCalibration, &Spout::FlowController::inform_flow_analysis_status };
        s_calibration_phase = CalibrationPhase::AnalysingFlowData;

        flow_controller.analyse_and_store_flow_data();
    }

    // if we have a scheduled calibration we execute it now
    // like i said, the flow of the code gets really nasty.
    if (s_scheduled_calibration_temperature) {
        LOG_IF(LogCalibration, "executando calibracao agendada");

        s_calibration_phase = CalibrationPhase::None;
        flow_controller.set_abort_analysis(false);
        calibrate(std::exchange(s_scheduled_calibration_temperature, std::nullopt));
        return;
    }

    s_calibration_phase = CalibrationPhase::Done;
    tone(BEEPER_PIN, 7000, 1000);
    LOG_IF(LogCalibration, "nivelamento finalizado");
}

CalibrationPhase calibration_phase() {
    return s_calibration_phase;
}

void inform_calibration_status() {
    info::send(
        info::Event::Calibration,
        [](JsonObject o) {
            o["needsCalibration"] = s_calibration_phase == CalibrationPhase::None;
        });
}

static usize s_new_firmware_size = 0;
static usize s_total_bytes_written = 0;

constexpr auto FIRMWARE_FILENAME = "Robin_nano_V3.bin";
static std::optional<storage::sd::File> s_firmware_file;

static void reset() {
    SERIAL_IMPL.flush();
    noInterrupts();
    NVIC_SystemReset();
}

static void firmware_update_failed(int error_code) {
    s_new_firmware_size = 0;
    s_total_bytes_written = 0;

    info::send(
        info::Event::Firmware,
        [error_code](JsonObject o) {
            o["updateFailedCode"] = error_code;
        });

    storage::sd::Card::the().delete_file(FIRMWARE_FILENAME);
    serial::FirmwareUpdateHook::the().deactivate();
}

static void add_buffer_to_new_firmware_file(std::span<char> buffer) {
    if (not s_firmware_file->write_binary(buffer)) {
        firmware_update_failed(0);
        return;
    }

    s_total_bytes_written += buffer.size();
    info::send(
        info::Event::Firmware,
        [](JsonObject o) {
            o["updateProgress"] = util::normalize(s_total_bytes_written, 0, s_new_firmware_size);
        });

    // we're done
    if (s_total_bytes_written == s_new_firmware_size) {
        if (not CFG(MaintenanceMode))
            Spout::FlowController::the().firmware_upgrade_finished();

        reset();
    }
}

void prepare_for_firmware_update(usize size) {
    s_new_firmware_size = size;
    s_firmware_file = storage::sd::Card::the().open_file(FIRMWARE_FILENAME, O_WRITE | O_CREAT | O_TRUNC | O_SYNC);
    if (not s_firmware_file) {
        firmware_update_failed(0);
        return;
    }

    serial::FirmwareUpdateHook::the().activate(&add_buffer_to_new_firmware_file, &firmware_update_failed, s_new_firmware_size);
    // TODO: purge all storage entries
}
}
