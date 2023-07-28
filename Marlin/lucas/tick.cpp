#include "tick.h"
#include <lucas/lucas.h>
#include <lucas/Estacao.h>
#include <lucas/Fila.h>
#include <lucas/Bico.h>
#include <lucas/info/info.h>

namespace lucas {
void tick() {
    static millis_t ultimo_tick = 0;
    // a taxa de atualização da máquina é de 1mhz, ou seja, 1 tick a cada 1 millisegundo
    if (ultimo_tick == millis())
        return;

    ultimo_tick = millis();

    { // fila
        if (not filtrado(Filtros::Fila))
            Fila::the().tick();

        Fila::the().remover_receitas_finalizadas();
    }

    { // estacoes
        if (not filtrado(Filtros::Estacao))
            Estacao::tick();

        Estacao::atualizar_leds();
    }

    { // bico
        if (not filtrado(Filtros::Bico))
            Bico::the().tick();
    }

    { // info
        if (not filtrado(Filtros::Info))
            info::tick();
    }
}
}
