#pragma once

#include <lucas/wifi/wifi.h>
#include <lucas/gcode/gcode.h>
#include <lucas/Bico.h>
#include <lucas/opcoes.h>
#include <lucas/tick.h>
#include <lucas/util/util.h>
#include <lucas/serial/serial.h>

namespace lucas {
#define LOG SERIAL_ECHOLNPGM

void setup();

void pos_execucao_gcode();
}
