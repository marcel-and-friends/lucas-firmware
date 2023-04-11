#pragma once

#include <kofy/wifi/wifi.h>
#include <kofy/gcode/gcode.h>
#include <kofy/Bico.h>
#include <kofy/opcoes.h>

namespace kofy {

#define DBG SERIAL_ECHOLNPGM
#define INFO(tipo, valor) DBG("kofy_info:", #tipo,":",valor);

inline bool g_mudando_boca_ativa = false;
inline bool g_conectando_wifi = false;
inline bool g_inicializando = false;

void setup();

void event_handler();

void idle();

}
