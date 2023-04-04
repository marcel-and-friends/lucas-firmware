#pragma once

#include <src/gcode/queue.h>
#include "Bico.h"
#include "wifi.h"
#include "opcoes.h"
#include "gcode.h"

namespace kofy {

#define DBG SERIAL_ECHOLNPGM
#define INFO(tipo, valor) DBG("kofy_info:", #tipo,":",valor);

inline bool g_mudando_boca_ativa = false;
inline bool g_conectando_wifi = false;
inline bool g_inicializando = false;

void setup();

void event_handler();

void idle();

namespace marlin {

inline bool apertado(int pino) {
    return READ(pino) == false;
}

}

}
