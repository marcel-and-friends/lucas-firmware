#pragma once

#include <src/gcode/queue.h>
#include "wifi.h"
#include "opcoes.h"
#include "gcode.h"

namespace kofy {

#define DBG SERIAL_ECHOLNPGM
#define INFO(tipo, valor) DBG("kofy_info:", #tipo,":",valor);

inline bool g_mudando_boca_ativa = false;
inline bool g_conectando_wifi = false;
inline bool g_inicializando = false;

struct Bico {
	bool ativo = false;
	uint32_t tempo = 0;
	int poder = 0;
	uint32_t tick = 0;
};

inline Bico g_bico{};

void setup();

void event_handler();

void idle();

namespace marlin {

void injetar_gcode(std::string_view gcode);

inline bool apertado(int pino) {
    return READ(pino) == false;
}

inline void parar_fila_de_gcode() {
    queue.injected_commands_P = nullptr;
}

}

}
