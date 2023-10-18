#include "core.h"
#include <lucas/lucas.h>
#include <lucas/Spout.h>
#include <lucas/Boiler.h>
#include <lucas/Station.h>
#include <lucas/MotionController.h>
#include <lucas/RecipeQueue.h>
#include <lucas/info/info.h>
#include <lucas/util/SD.h>
#include <lucas/serial/FirmwareUpdateHook.h>
#include <src/module/planner.h>

namespace lucas::core {
static bool s_calibrated = false;
static auto s_calibration_phase = CalibrationPhase::None;

void setup() {
    planner.settings.axis_steps_per_mm[X_AXIS] = util::DEFAULT_STEPS_PER_MM_X;
    planner.settings.axis_steps_per_mm[Y_AXIS] = util::DEFAULT_STEPS_PER_MM_Y;

    Boiler::the().setup();
    Spout::the().setup();
    MotionController::the().home();
    MotionController::the().travel_to_sewer();
    RecipeQueue::the().setup();

    // remove these two lines when we get to prod!
    Station::initialize(3);
    calibrate(93);

    inform_calibration_status();
}

void tick() {
    // boiler
    if (not is_filtered(core::Filter::Boiler))
        Boiler::the().tick();

    // queue
    if (not is_filtered(core::Filter::RecipeQueue))
        RecipeQueue::the().tick();

    RecipeQueue::the().remove_finalized_recipes();

    // stations
    if (not is_filtered(core::Filter::Station))
        Station::tick();

    Station::update_leds();

    // spout
    if (not is_filtered(core::Filter::Spout))
        Spout::the().tick();
}

void calibrate(float target_temperature) {
    LOG_IF(LogCalibration, "iniciando nivelamento");
    s_calibrated = true;

    if (CFG(SetTargetTemperatureOnCalibration)) {
        s_calibration_phase = CalibrationPhase::WaitingForTemperatureToStabilize;
        Boiler::the().set_target_temperature_and_wait(target_temperature);
    }

    if (CFG(FillDigitalSignalTableOnCalibration)) {
        s_calibration_phase = CalibrationPhase::FillingDigitalSignalTable;
        Spout::FlowController::the().fill_digital_signal_table();
    } else {
        if (util::SD::make().file_exists(Spout::FlowController::TABLE_FILE_PATH))
            Spout::FlowController::the().fetch_digital_signal_table_from_file();
    }

    s_calibration_phase = CalibrationPhase::None;
    LOG_IF(LogCalibration, "nivelamento finalizado");
}

void inform_calibration_status() {
    info::send(
        info::Event::Calibration,
        [](JsonObject o) {
            o["needsCalibration"] = not s_calibrated;
        });

    switch (s_calibration_phase) {
    case CalibrationPhase::WaitingForTemperatureToStabilize: {
        Boiler::the().inform_temperature_to_host();
    } break;
    case CalibrationPhase::FillingDigitalSignalTable: {
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
    if (not s_sd.write_from(buffer)) {
        LOG_ERR("falha ao escrever parte do firmware");
        return;
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
