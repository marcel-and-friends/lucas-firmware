#include "lucas.h"
#include <lucas/Bico.h>
#include <lucas/Fila.h>
#include <lucas/serial/serial.h>
#include <lucas/info/info.h>
#include <lucas/wifi/wifi.h>

namespace lucas {
// https://www.st.com/resource/en/reference_manual/rm0090-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf

void setup() {
    util::FiltroUpdatesTemporario f{ Filtros::Todos };

    serial::setup();
    info::setup();

    Bico::the().setup();
    Fila::the().setup();

    if (CFG(ConectarWifiAuto))
        wifi::conectar("Kika-Amora", "Desconto5");
}
}
