#pragma once

#include <lucas/wifi/wifi.h>
#include <lucas/gcode/gcode.h>
#include <lucas/Bico.h>
#include <lucas/opcoes.h>
#include <lucas/tick.h>

namespace lucas {
#define LOG SERIAL_ECHOLNPGM
#define UPDATE(tipo, valor) LOG("$", tipo, ":", valor);

inline bool g_trocando_estacao_ativa = false;
inline bool g_conectando_wifi = false;
inline bool g_executando_rotina_inicial = false;

void setup();

void pos_execucao_gcode();

}
