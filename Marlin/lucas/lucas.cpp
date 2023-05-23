#include "lucas.h"
#include <lucas/Estacao.h>

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

    LOG("iniciando");
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
