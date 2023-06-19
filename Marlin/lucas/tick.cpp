#include "tick.h"
#include <lucas/lucas.h>
#include <lucas/Estacao.h>
#include <lucas/Fila.h>
#include <lucas/Bico.h>
#include <lucas/info/info.h>

namespace lucas {
void tick() {
    static millis_t ultimo_tick = 0;
    auto tick = millis();
    //  a 'idle' pode ser chamada mais de uma vez em um milésimo
    //  precisamos fitrar esses casos para nao mudarmos o estado das estacoes/leds mais de uma vez por ms
    if (ultimo_tick == tick)
        return;

    ultimo_tick = tick;

    Fila::the().tick();
    Estacao::tick();
    Bico::the().tick();
    info::tick();
}
}
