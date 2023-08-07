#include "lucas.h"
#include <lucas/core/core.h>
#include <lucas/Bico.h>
#include <lucas/Boiler.h>
#include <lucas/Fila.h>
#include <lucas/serial/serial.h>
#include <lucas/info/info.h>
#include <lucas/wifi/wifi.h>

namespace lucas {
static bool s_inicializado = false;

void setup() {
    util::FiltroUpdatesTemporario f{ Filtros::Interacao };

    cfg::setup();
    serial::setup();
    info::setup();
    core::setup();

    s_inicializado = true;
}

bool inicializado() {
    return s_inicializado;
}
}
