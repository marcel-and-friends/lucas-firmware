#include "Bico.h"
#include <lucas/lucas.h>
#include <src/module/temperature.h>

namespace lucas {
static constexpr auto PINO = MT_DET_1_PIN;
static constexpr auto PINO_ENABLE = MT_DET_2_PIN;

void Bico::agir(millis_t tick) {
    if (!s_ativo)
        return;

    if (tick - s_tick < s_tempo) {
        analogWrite(PINO_ENABLE, 0);
        analogWrite(PINO, s_poder);
    } else {
        LOG("Finalizando bico ", s_poder, " no tick: ", tick, " - diff: ", tick - s_tick, " - ideal: ", s_tempo);
        reset();
    }
}

void Bico::reset() {
    analogWrite(PINO_ENABLE, 255);
    analogWrite(PINO, 0);
    s_ativo = false;
    s_poder = 0;
    s_tick = 0;
    s_tempo = 0;
}

void Bico::setup() {
    _SET_OUTPUT(PINO);
    _SET_OUTPUT(PINO_ENABLE);
    reset();
}

void Bico::ativar(millis_t tick, millis_t tempo, float poder) {
    auto correcao = [](float poder) {
        return 1.2f * poder - 20.f;
    };
    auto corrigido = correcao(poder);
    auto valor_digital = std::lerp(0.f, 255.f, corrigido / 100.f);
    s_poder = std::lroundf(valor_digital);
    s_ativo = true;
    s_tick = tick;
    s_tempo = tempo;
    LOG("Começando bico -> T: ", s_tempo, " - P: ", s_poder, " - tick: ", s_tick);
}

}
