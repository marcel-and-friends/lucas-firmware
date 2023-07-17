#include "tick.h"
#include <lucas/lucas.h>
#include <lucas/Estacao.h>
#include <lucas/Fila.h>
#include <lucas/Bico.h>
#include <lucas/info/info.h>

namespace lucas {
void tick() {
    static millis_t ultimo_tick = 0;
    //  a 'idle' pode ser chamada mais de uma vez em um milésimo
    //  precisamos fitrar esses casos para nao mudarmos o estado das estacoes/leds mais de uma vez por ms
    if (ultimo_tick == millis())
        return;

    ultimo_tick = millis();

    { // fila
        if (!filtrado(Filtros::Fila))
            Fila::the().tick();

        Fila::the().remover_receitas_finalizadas();
    }

    { // estacoes
        if (!filtrado(Filtros::Estacao))
            Estacao::tick();

        Estacao::atualizar_leds();
    }

    { // bico
        if (!filtrado(Filtros::Bico))
            Bico::the().tick();
    }

    { // info
        if (!filtrado(Filtros::Info))
            info::tick();
    }
}
}
