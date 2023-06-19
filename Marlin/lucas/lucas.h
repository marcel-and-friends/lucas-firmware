#pragma once

#include <lucas/opcoes.h>
#include <lucas/tick.h>
#include <lucas/util/util.h>

namespace lucas {
#define LOG SERIAL_ECHOLNPGM

void setup();

void pos_execucao_gcode();
}
