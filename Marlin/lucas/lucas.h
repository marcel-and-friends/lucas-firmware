#pragma once

#include <lucas/wifi/wifi.h>
#include <lucas/gcode/gcode.h>
#include <lucas/Bico.h>
#include <lucas/opcoes.h>

namespace lucas {

#define DBG SERIAL_ECHOLNPGM
#define INFO(tipo, valor) DBG("lucas_info:", #tipo,":",valor);

inline bool g_mudando_boca_ativa = false;
inline bool g_conectando_wifi = false;
inline bool g_inicializando = false;

void setup();

void event_handler();

void idle();

}
