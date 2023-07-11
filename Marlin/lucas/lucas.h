#pragma once

#include <lucas/opcoes.h>
#include <lucas/tick.h>
#include <lucas/util/util.h>
#include <lucas/cfg/cfg.h>

namespace lucas {
#define LOG SERIAL_ECHOLNPGM
#define LOG_ERR(...) LOG("", "ERRO: ", __VA_ARGS__);

void setup();
}
