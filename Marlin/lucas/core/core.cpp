#include "core.h"
#include <lucas/lucas.h>
#include <lucas/Spout.h>
#include <lucas/Boiler.h>
#include <lucas/serial/FirmwareUpdateHook.h>
#include <lucas/Station.h>
#include <lucas/RecipeQueue.h>
#include <src/sd/cardreader.h>
#include <src/MarlinCore.h>

namespace lucas::core {
constexpr auto CALIBRATION_KEYWORD = "jujuba";
static bool s_calibrated = false;

void setup() {
    Spout::the().setup();
    request_calibration();
}

void calibrate(float target_temperature) {
    LOG_IF(LogCalibration, "iniciando rotina de nivelamento");

    Spout::the().home();

    if (CFG(SetTargetTemperatureOnCalibration))
        Boiler::the().set_target_temperature_and_wait(target_temperature);

    if (CFG(FillDigitalSignalTableOnCalibration))
        Spout::FlowController::the().fill_digital_signal_table();
    else
        Spout::FlowController::the().try_fetching_digital_signal_table_from_flash();

    LOG_IF(LogCalibration, "nivelamento finalizado");

    s_calibrated = true;
}

bool calibrated() {
    return s_calibrated;
}

void request_calibration() {
    SERIAL_ECHOLN(CALIBRATION_KEYWORD);
}

static size_t s_new_firmware_size = 0;
static size_t s_total_bytes_written = 0;
constexpr auto FIRMWARE_FILE_PATH = "/Robin_nano_V3.bin";

static void prepare_sd_card(bool delete_if_file_exists) {
    if (not card.isMounted()) {
        LOG("montando o cartao sd");
        card.mount();
    }

    if (card.isFileOpen())
        card.closefile();

    if (delete_if_file_exists) {
        if (card.fileExists(FIRMWARE_FILE_PATH))
            card.removeFile(FIRMWARE_FILE_PATH);
    }

    card.openFileWrite(FIRMWARE_FILE_PATH);
    LOG("arquivo aberto para escrita");
}

static void add_buffer_to_new_firmware_file(std::span<char> buffer) {
    if (not card.isMounted() or not card.isFileOpen()) {
        LOG("cartao nao estava pronto para receber o firmware, tentando arrumar");
        prepare_sd_card(false);
        return;
    }

    auto const bytes_to_write = static_cast<int16_t>(buffer.size());
    auto bytes_written = card.write(buffer.data(), buffer.size());
    auto attempts = 0;
    while (bytes_written == -1 and attempts++ < 5) {
        LOG_ERR("erro ao escrever parte do firmware no cartao sd, tentando novamente");
        bytes_written = card.write(buffer.data(), buffer.size());
    }

    if (bytes_written == -1)
        return;

    s_total_bytes_written += bytes_written;
    LOG("escrito ", bytes_written, " bytes");

    if (s_total_bytes_written == s_new_firmware_size) {
        card.closefile();
        noInterrupts();
        NVIC_SystemReset();
        __builtin_unreachable();
    }
}

void prepare_for_firmware_update(size_t size) {
    s_new_firmware_size = size;

    prepare_sd_card(true);

    serial::FirmwareUpdateHook::create(
        &add_buffer_to_new_firmware_file,
        s_new_firmware_size);

    LOG("novo firmware tera ", s_new_firmware_size, " bytes");
}
}
