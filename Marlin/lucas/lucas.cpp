#include "lucas.h"
#include <lucas/Bico.h>
#include <lucas/Fila.h>
#include <lucas/serial/serial.h>
#include <lucas/info/info.h>
#include <lucas/wifi/wifi.h>

namespace lucas {
void setup() {
    LOG("inicializando a maquina");

    util::FiltroUpdatesTemporario f{ Filtros::Todos };

    serial::setup();
    info::setup();

    Bico::the().setup();
    Fila::the().setup();

    if (CFG(ConectarWifiAuto))
        wifi::conectar("Kika-Amora", "Desconto5");
}
}
