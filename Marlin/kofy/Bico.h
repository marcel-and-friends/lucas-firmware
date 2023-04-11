#pragma once

#include <src/MarlinCore.h>

namespace kofy {

class Bico {
public:
	static constexpr auto PINO = FAN_PIN;

	static void agir(millis_t tick);

	static void ativar(millis_t tick, millis_t tempo, int poder);

	static void reset();

	static bool ativo() {
		return s_ativo;
	}

private:
	static inline millis_t s_tempo = 0;

	static inline millis_t s_tick = 0;

	static inline int s_poder = 0;

	static inline bool s_ativo = false;

};

}
