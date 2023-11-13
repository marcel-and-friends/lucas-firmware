#include "core.h"
#include <utility>
#include <lucas/lucas.h>
#include <lucas/Spout.h>
#include <lucas/Boiler.h>
#include <lucas/Station.h>
#include <lucas/MotionController.h>
#include <lucas/RecipeQueue.h>
#include <lucas/info/info.h>
#include <lucas/serial/serial.h>
#include <lucas/util/SD.h>
#include <lucas/serial/FirmwareUpdateHook.h>
#include <src/module/planner.h>

namespace lucas::core {
static auto s_calibration_phase = CalibrationPhase::None;
static auto s_scheduled_calibration_temperature = 0;
static util::Timer s_time_since_setup = {};

void setup() {
    MotionController::the().setup();

    if (CFG(MaintenanceMode)) {
        Spout::the().setup_pins();
        Boiler::the().setup_pins();
        Station::initialize(5);
        return;
    }

    Boiler::the().setup();
    Spout::the().setup();
    MotionController::the().home();
    MotionController::the().travel_to_sewer();
    RecipeQueue::the().setup();
}

void tick() {
    if (CFG(MaintenanceMode)) {
        // we need this for button press logs
        Station::tick();
        if (Boiler::the().target_temperature())
            Boiler::the().tick();
        return;
    }

    if (not s_time_since_setup.is_active()) {
        inform_calibration_status();
        s_time_since_setup.start();
        RecipeQueue::the().reset_inactivity();
    }

    // 30 seconds have past and the app hasn't sent a calibration request
    // we're probably on our own then
    if (s_time_since_setup >= 30s and s_calibration_phase == CalibrationPhase::None) {
        LOG("calibracao automatica iniciada");
        // FIXME: get the amount of stations from somewhere
        Station::initialize(3);
        calibrate(94);
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
}

void calibrate(float target_temperature) {
    if (target_temperature == Boiler::the().target_temperature())
        return;

    // maybe we are already calibrating...
    switch (s_calibration_phase) {
    // if we're still just reaching the target temp just update the target temperature and keep waiting
    case CalibrationPhase::ReachingTargetTemperature:
        LOG_IF(LogCalibration, "trocando temperatura target");
        Boiler::the().update_target_temperature(target_temperature);
        return;
    // if we're doing flow analysis it gets a bit more complex
    // we tell the flow controller it should abort, wait for it's loop to be called again and save the new desired temperature for later
    // "later" in this case is a few lines below, where `s_scheduled_calibration_temperature` is used.
    // the flow of the code gets really nasty in the current architecture, need to rewrite everything.
    case CalibrationPhase::AnalysingFlowData:
        LOG_IF(LogCalibration, "cancelando analise de fluxo");
        Spout::FlowController::the().set_abort_analysis(true);
        s_scheduled_calibration_temperature = target_temperature;
        return;
    }

    LOG_IF(LogCalibration, "iniciando nivelamento");

    if (CFG(SetTargetTemperatureOnCalibration)) {
        s_calibration_phase = CalibrationPhase::ReachingTargetTemperature;
        Boiler::the().update_and_reach_target_temperature(target_temperature);
    }

    if (CFG(FillDigitalSignalTableOnCalibration)) {
        s_calibration_phase = CalibrationPhase::AnalysingFlowData;
        Spout::FlowController::the().analyse_and_store_flow_data();
    } else {
        if (util::SD::make().file_exists(Spout::FlowController::TABLE_FILE_PATH))
            Spout::FlowController::the().fetch_digital_signal_table_from_file();
    }

    // if we have a scheduled calibration we execute it now
    // like i said, the flow of the code gets really nasty.
    if (s_scheduled_calibration_temperature) {
        LOG_IF(LogCalibration, "executando calibracao agendada");
        s_calibration_phase = CalibrationPhase::None;
        Spout::FlowController::the().set_abort_analysis(false);
        calibrate(std::exchange(s_scheduled_calibration_temperature, 0));
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

    switch (s_calibration_phase) {
    case CalibrationPhase::ReachingTargetTemperature: {
        Boiler::the().inform_temperature_to_host();
    } break;
    case CalibrationPhase::AnalysingFlowData: {
        Spout::FlowController::the().inform_progress_to_host();
    } break;
    default:
        break;
    }
}

static usize s_new_firmware_size = 0;
static usize s_total_bytes_written = 0;
static util::SD s_sd;
constexpr auto FIRMWARE_FILE_PATH = "/Robin_nano_V3.bin";

static void add_buffer_to_new_firmware_file(std::span<char> buffer) {
    for (auto attempts = 0; attempts <= 5; ++attempts) {
        if (s_sd.write_from(buffer))
            break;
        LOG_ERR("falha ao escrever parte do firmware, tentando novamente...");
    }

    s_total_bytes_written += buffer.size();
    info::send(
        info::Event::Firmware,
        [](JsonObject o) {
            o["updateProgress"] = util::normalize(s_total_bytes_written, 0, s_new_firmware_size);
        });

    if (s_total_bytes_written == s_new_firmware_size) {
        SERIAL_IMPL.flush();
        s_sd.close();
        noInterrupts();
        NVIC_SystemReset();
    }
}

void prepare_for_firmware_update(usize size) {
    s_new_firmware_size = size;

    // cfg::reset_options();
    // Spout::FlowController::the().clean_digital_signal_table(Spout::FlowController::RemoveFile::Yes);
    serial::FirmwareUpdateHook::the().activate(&add_buffer_to_new_firmware_file, s_new_firmware_size);

    if (not s_sd.open(FIRMWARE_FILE_PATH, util::SD::OpenMode::Write)) {
        LOG_ERR("falha ao abrir novo arquivo do firmware");
        return;
    }
}
}
