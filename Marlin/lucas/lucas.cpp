#include "lucas.h"
#include <lucas/Estacao.h>
#include <lucas/serial/HookDelimitado.h>
#include <lucas/info/info.h>
#include <lucas/Fila.h>
#include <ArduinoJson.h>

namespace lucas {
void setup() {
    LOG("iniciando lucas");

    serial::HookDelimitado::make('#', [](std::span<char> buffer) {
        info::interpretar_json(buffer);
    });

    Bico::the().setup();
#if LUCAS_CONECTAR_WIFI
    wifi::conectar(LUCAS_WIFI_NOME_SENHA);
#endif
}

void pos_execucao_gcode() {
    if (wifi::conectando()) {
        if (wifi::terminou_de_conectar())
            wifi::informar_sobre_rede();
        return;
    }
}
}
