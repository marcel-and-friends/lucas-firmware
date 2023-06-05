#include "lucas.h"
#include <lucas/Estacao.h>
#include <lucas/serial/HookDelimitado.h>
#include <ArduinoJson.h>

namespace lucas {
void setup() {
    struct InfoEstacao {
        int pino_botao;
        int pino_led;
    };

    static constexpr std::array<InfoEstacao, Estacao::NUM_ESTACOES> infos = {
        InfoEstacao{.pino_botao = PC8,  .pino_led = PE13},
        InfoEstacao{ .pino_botao = PC4, .pino_led = PD13}
    };

    for (size_t i = 0; i < Estacao::NUM_ESTACOES; i++) {
        auto& info = infos.at(i);
        auto& estacao = Estacao::lista().at(i);
        estacao.set_botao(info.pino_botao);
        estacao.set_led(info.pino_led);
        // todas as maquinas começam livres
        estacao.set_livre(true);
    }

    UPDATE(LUCAS_UPDATE_NUM_ESTACOES, Estacao::NUM_ESTACOES)
    LOG("iniciando");

    serial::HookDelimitado::make('#', [](std::span<const char> buffer) {
        StaticJsonDocument<1024> doc;
        auto err = deserializeJson(doc, buffer.data(), buffer.size());
        if (err) {
            LOG("desserializacao json falhou - [", err.c_str(), "]");
            return;
        }
        LOG("doc[\"oi\"] = ", doc["oi"].as<const char*>());
    });

    serial::HookDelimitado::make('%', [](std::span<const char> buffer) {
        LOG("gcode = ", buffer.data());
    });

    serial::HookDelimitado::make('$', [](std::span<const char> buffer) {
        LOG("coisa = ", buffer.data());
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

    if (!Estacao::ativa())
        return;

    Estacao::ativa()->prosseguir_receita();
}
}
