#include "core.h"
#include <lucas/lucas.h>
#include <lucas/Bico.h>
#include <lucas/Boiler.h>
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

void add_buffer_to_new_firmware_file(std::span<char> buffer) {
    static bool open_once = false;
    if (not open_once) {
        if (not card.isMounted())
            card.mount();
        card.openFileWrite("Robin_Nano_V3.bin");
        open_once = true;
    }

    auto res = card.write(buffer.data(), buffer.size());
    while (res == -1 or res != buffer.size()) {
        LOG_ERR("erro ao escrever parte do firmware no cartao sd - tentando novamente");
        res = card.write(buffer.data(), buffer.size());
    }

    if (buffer.size() != 1024) {
        LOG_ERR("finalizando escrita do firmware no cartao sd");
        card.closefile();
        NVIC_SystemReset();
    }
}
}
