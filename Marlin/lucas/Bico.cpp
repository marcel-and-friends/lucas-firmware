#include "Bico.h"
#include <lucas/lucas.h>
#include <src/module/temperature.h>

namespace lucas {
static constexpr auto PINO = PC14; // FAN_PIN

void Bico::agir(millis_t tick) {
    if (s_ativo) {
        if (tick - s_tick <= s_tempo) {
            thermalManager.set_fan_speed(0, s_poder);
        } else {
            DBG("finalizando bico com valor ", s_poder, " no tick: ", tick, " - diff: ", tick - s_tick, " - ideal: ", s_tempo);
            reset();
        }
    } else {
        thermalManager.set_fan_speed(0, 0);
    }
}

void Bico::reset() {
    thermalManager.set_fan_speed(0, 0);
    s_ativo = false;
    s_poder = 0;
    s_tick = 0;
    s_tempo = 0;
}

void Bico::ativar(millis_t tick, millis_t tempo, int poder) {
    s_ativo = true;
    s_tick = tick;
    s_tempo = tempo;
    s_poder = poder;

    DBG("T: ", s_tempo, " - P: ", s_poder, " - tick: ", s_tick);
}

}
