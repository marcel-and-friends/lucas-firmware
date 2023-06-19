#include "lucas.h"
#include <lucas/serial/HookDelimitado.h>
#include <lucas/info/info.h>
#include <lucas/wifi/wifi.h>
#include <lucas/Bico.h>
#include <lucas/Estacao.h>
#include <src/module/temperature.h>
#include <lucas/Fila.h>

namespace lucas {
enum Intervalos : millis_t {
    Boiler = 5000,
    EstacaoAtiva = 500
};

void setup() {
    LOG("iniciando lucas");

    Bico::the().setup();

    serial::HookDelimitado::make(
        '#',
        [](std::span<char> buffer) {
            info::interpretar_json(buffer);
        });

    info::Report::make(
        "infoBoiler",
        Intervalos::Boiler,
        [](millis_t tick, JsonObject obj) {
            obj["tempAtual"] = thermalManager.degBed();
        });

    info::Report::make(
        "infoFila",
        Intervalos::EstacaoAtiva,
        [](millis_t tick, JsonObject obj) {
            Fila::the().gerar_info(tick, obj);
        },
        [] {
            return Fila::the().executando();
        });

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
