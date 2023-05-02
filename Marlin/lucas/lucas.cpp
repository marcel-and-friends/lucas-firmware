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
        InfoEstacao{ .pino_botao = PA4,  .pino_led = PE8 },
        InfoEstacao{ .pino_botao = PE6,  .pino_led = PE15},
    };

    for (size_t i = 0; i < Estacao::NUM_ESTACOES; i++) {
        auto& info = infos.at(i);
        auto& estacao = Estacao::lista().at(i);

        estacao.set_botao(info.pino_botao);
        estacao.set_led(info.pino_led);
        estacao.set_livre(true);
    }

    LOG("iniciando! - numero de estacoes = ", Estacao::NUM_ESTACOES);
#if LUCAS_CONECTAR_WIFI
    wifi::conectar(LUCAS_WIFI_NOME_SENHA);
#endif
}

static bool pronto_para_prosseguir_receita();

void pos_execucao_gcode() {
    if (!pronto_para_prosseguir_receita())
        return;

    if (Bico::ativo())
        return;

#ifdef LUCAS_ROUBAR_FILA_GCODE
    gcode::injetar(LUCAS_ROUBAR_FILA_GCODE);
    return;
#endif

    if (!Estacao::ativa())
        return;

    auto& estacao = *Estacao::ativa();
    if (estacao.livre()) {
        // a receita acabou
        estacao.atualizar_status("Livre");
        estacao.atualizar_campo_gcode(Estacao::CampoGcode::Atual, "-");
        Estacao::procurar_nova_ativa();
    } else {
        estacao.prosseguir_receita();
    }
}

static void executar_rotina_inicial() {
    LOG("executando rotina inicial");
    gcode::injetar(gcode::ROTINA_INICIAL);
#if LUCAS_DEBUG_GCODE
    LOG("---- gcode da rotina ----\n", gcode::ROTINA_INICIAL);
    LOG("-------------------------");
#endif
    g_executando_rotina_inicial = true;
}

static bool pronto_para_prosseguir_receita() {
#if LUCAS_CONECTAR_WIFI
    if (wifi::terminou_de_conectar()) {
        wifi::informar_sobre_rede();
    #if LUCAS_ROTINA_INICIAL
        executar_rotina_inicial();
    #endif
    }

#else
    if (!g_executando_rotina_inicial)
        executar_rotina_inicial();
#endif

    if (g_executando_rotina_inicial) {
        if (!gcode::tem_comandos_pendentes()) {
            g_executando_rotina_inicial = false;
            LOG("rotina inicial finalizada.");
        }
    }

    if (gcode::roubando_fila())
        if (!gcode::tem_comandos_pendentes())
            gcode::fila_roubada_terminou();

    if (Estacao::trocando_de_estacao_ativa())
        if (!gcode::tem_comandos_pendentes())
            Estacao::ativa_prestes_a_comecar();

    return !Estacao::trocando_de_estacao_ativa() && !gcode::roubando_fila() && !g_executando_rotina_inicial;
}
}
