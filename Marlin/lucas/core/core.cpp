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

void setup() {
    planner.settings.axis_steps_per_mm[X_AXIS] = util::DEFAULT_STEPS_PER_MM_X;
    planner.settings.axis_steps_per_mm[Y_AXIS] = util::DEFAULT_STEPS_PER_MM_Y;

    Boiler::the().setup();
    Spout::the().setup();
    MotionController::the().home();

    inform_calibration_status();
}

void calibrate(float target_temperature) {
    core::TemporaryFilter f{ TickFilter::RecipeQueue & TickFilter::Station };
    LOG_IF(LogCalibration, "iniciando nivelamento");
    s_calibrated = true;

    MotionController::the().home();

    if (CFG(SetTargetTemperatureOnCalibration))
        Boiler::the().set_target_temperature_and_wait(target_temperature);

    if (CFG(FillDigitalSignalTableOnCalibration)) {
        Spout::FlowController::the().fill_digital_signal_table();
    } else {
        if (util::SD::make().file_exists(Spout::FlowController::TABLE_FILE_PATH))
            Spout::FlowController::the().fetch_digital_signal_table_from_file();
    }

    LOG_IF(LogCalibration, "nivelamento finalizado");
}

void inform_calibration_status() {
    info::send(
        info::Event::Calibration,
        [](JsonObject o) {
            o["needsCalibration"] = not s_calibrated;
        });
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
