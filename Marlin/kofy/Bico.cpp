#include "Bico.h"
#include <kofy/kofy.h>

namespace kofy {
void Bico::agir(millis_t tick) {
	if (s_ativo) {
		if (tick - s_tick <= s_tempo) {
			WRITE(PINO, s_poder);
		} else {
			DBG("finalizando troço no tick: ", tick, " - diff: ", tick - s_tick, " - ideal: ", s_tempo);
			reset();
		}
	} else {
		WRITE(PINO, 0);
	}

}

void Bico::reset() {
	WRITE(PINO, 0);
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
}

void Bico::debug() {
	DBG("T: ", s_tempo, " - P: ", s_poder, " - tick: ", s_tick);
}

}
