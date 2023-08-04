#include "lucas.h"
#include <lucas/Bico.h>
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

    Bico::the().setup();

    serial::limpar_serial();
    SERIAL_ECHOLN("jujuba");

    s_inicializado = true;
}

bool inicializado() {
    return s_inicializado;
}
}
