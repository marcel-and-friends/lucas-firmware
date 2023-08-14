#include "core.h"
#include <lucas/lucas.h>
#include <lucas/Bico.h>
#include <lucas/Boiler.h>
#include <lucas/serial/UniquePriorityHook.h>
#include <src/sd/cardreader.h>
#include <src/MarlinCore.h>

namespace lucas::core {
constexpr auto NIVELAMENTO_KEYWORD = "jujuba";
static bool s_nivelado = false;

void setup() {
    Bico::the().setup();

    solicitar_nivelamento();
}

void nivelar(float temperatura_target) {
    util::FiltroUpdatesTemporario f{ Filtros::Interacao };

    LOG_IF(LogNivelamento, "iniciando rotina de nivelamento");

    Bico::the().home();

    if (CFG(SetarTemperaturaTargetNoNivelamento))
        Boiler::the().aguardar_temperatura(temperatura_target);

    if (CFG(PreencherTabelaDeFluxoNoNivelamento))
        Bico::ControladorFluxo::the().preencher_tabela();
    else
        Bico::ControladorFluxo::the().tentar_copiar_tabela_da_flash();

    LOG_IF(LogNivelamento, "nivelamento finalizado");

    s_nivelado = true;
}

bool nivelado() {
    return s_nivelado;
}

void solicitar_nivelamento() {
    SERIAL_ECHOLN(NIVELAMENTO_KEYWORD);
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
    auto const bytes_written = card.write(buffer.data(), buffer.size());
    if (bytes_written == -1) {
        LOG_ERR("erro ao escrever parte do firmware no cartao sd");
        return;
    }

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

    serial::UniquePriorityHook::set(
        [](std::span<char> buffer) {
            add_buffer_to_new_firmware_file(buffer);
        },
        size);

    LOG("novo firmware tera ", s_new_firmware_size, " bytes");
}
}
