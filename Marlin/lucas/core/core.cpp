#include "core.h"
#include <lucas/lucas.h>
#include <lucas/Bico.h>
#include <lucas/Boiler.h>

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
}
