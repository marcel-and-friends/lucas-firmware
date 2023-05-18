#pragma once

#include <lucas/wifi/wifi.h>
#include <lucas/gcode/gcode.h>
#include <lucas/Bico.h>
#include <lucas/opcoes.h>
#include <lucas/tick.h>

namespace lucas {
#define LOG SERIAL_ECHOLNPGM
#define UPDATE(tipo, valor) LOG("$", tipo, ":", valor);

void setup();

void pos_execucao_gcode();
}
