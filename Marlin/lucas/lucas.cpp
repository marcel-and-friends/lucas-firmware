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
    g_conectando_wifi = true;
    LOG("conectando wifi");
    wifi::conectar(LUCAS_WIFI_NOME_SENHA);
#endif
}

static bool pronto();

void pos_execucao_gcode() {
    if (!pronto())
        return;

    // não prosseguimos com a receita se o bico está ativo
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

static bool pronto() {
#if LUCAS_CONECTAR_WIFI
    if (g_conectando_wifi) {
        if (wifi::conectado()) {
            g_conectando_wifi = false;
            wifi::terminou_de_conectar();
    #if LUCAS_ROTINA_INICIAL
            executar_rotina_inicial();
    #endif
        }
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

    if (g_trocando_estacao_ativa) {
        if (!gcode::tem_comandos_pendentes()) {
            g_trocando_estacao_ativa = false;
            LOG("troca de estacao finalizada, bico esta na nova posicao");
            Estacao::ativa_prestes_a_comecar();
        }
    }

    return !g_conectando_wifi && !g_executando_rotina_inicial && !g_trocando_estacao_ativa;
}
}
