#include "core.h"
#include <lucas/lucas.h>
#include <lucas/Spout.h>
#include <lucas/Boiler.h>
#include <lucas/serial/UniquePriorityHook.h>
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
    util::TemporaryFilter f{ Filters::Interaction };

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

static bool s_is_updating_firmware = false;
static size_t s_new_firmware_size = 0;

static void add_buffer_to_new_firmware_file(std::span<char> buffer) {
    if (not card.isFileOpen()) {
        if (not card.isMounted()) {
            LOG("tentando montar o cartao sd");
            card.mount();
        }

        if (card.isMounted()) {
            card.openFileWrite("/Robin_nano_V3.bin");
            LOG("arquivo aberto para escrita");
        }
    }

    if (not card.isFileOpen()) {
        LOG_ERR("o arquivo nao esta aberto");
        return;
    }

    static size_t s_total_bytes_written = 0;
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
        serial::UniquePriorityHook::unset();

        s_is_updating_firmware = false;
        s_total_bytes_written = 0;
        s_new_firmware_size = 0;

        card.closefile();

        LOG("firmware atualizado com sucesso - reiniciando");

        noInterrupts();
        NVIC_SystemReset();
        while (true) {
        }
    }
}

void prepare_for_firmware_update(size_t size) {
    s_new_firmware_size = size;
    s_is_updating_firmware = true;

    serial::UniquePriorityHook::set(&add_buffer_to_new_firmware_file, size);

    LOG("novo firmware tera ", s_new_firmware_size, " bytes");
}
}
