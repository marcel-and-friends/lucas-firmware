#include "lucas.h"
#include <lucas/Estacao.h>

namespace lucas {
void setup() {
    struct InfoEstacao {
        int pino_botao;
        int pino_led;
    };

    static constexpr std::array<InfoEstacao, Estacao::NUM_ESTACOES> infos = {
        InfoEstacao{.pino_botao = PC8,   .pino_led = PE13},
        InfoEstacao{ .pino_botao = PC4,  .pino_led = PD13},
        InfoEstacao{ .pino_botao = PA13, .pino_led = PC6 },
        InfoEstacao{ .pino_botao = 0,    .pino_led = PE8 },
        InfoEstacao{ .pino_botao = PB2,  .pino_led = PE15},
    };

    for (size_t i = 0; i < Estacao::NUM_ESTACOES; i++) {
        auto& info = infos.at(i);
        auto& estacao = Estacao::lista().at(i);
        estacao.set_botao(info.pino_botao);
        estacao.set_led(info.pino_led);
        estacao.set_livre(true);
    }

    LOG("iniciando! - numero de estacoes = ", Estacao::NUM_ESTACOES);
    Bico::setup();
#if LUCAS_CONECTAR_WIFI
    wifi::conectar(LUCAS_WIFI_NOME_SENHA);
#endif
}

static bool pronto_para_prosseguir_receita();

void pos_execucao_gcode() {
    if (!pronto_para_prosseguir_receita())
        return;

#ifdef LUCAS_ROUBAR_FILA_GCODE
    gcode::injetar(LUCAS_ROUBAR_FILA_GCODE);
    return;
#endif

    if (!Estacao::ativa())
        return;

    auto& estacao = *Estacao::ativa();
    // ultimo gcode acabou de ser executado
    if (estacao.livre()) {
        estacao.receita_finalizada();
    } else {
        estacao.prosseguir_receita();
    }
}

static void executar_rotina_de_nivelamento() {
    LOG("executando rotina de nivelamento");
    gcode::injetar(gcode::ROTINA_NIVELAMENTO);
#if LUCAS_DEBUG_GCODE
    LOG("---- gcode da rotina ----\n", gcode::ROTINA_NIVELAMENTO);
    LOG("-------------------------");
#endif
    g_nivelando = true;
}

static bool pronto_para_prosseguir_receita() {
#if LUCAS_CONECTAR_WIFI
    if (wifi::terminou_de_conectar()) {
        wifi::informar_sobre_rede();
    #if LUCAS_ROTINA_INICIAL
        executar_rotina_de_nivelamento();
    #endif
    }

#else
    if (!g_nivelando)
        executar_rotina_de_nivelamento();
#endif

    if (g_nivelando) {
        if (!gcode::tem_comandos_pendentes()) {
            g_nivelando = false;
            LOG("rotina de nivelamento finalizada.");
        }
    }

    return !g_nivelando;
}
}
