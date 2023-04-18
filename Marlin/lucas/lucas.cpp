#include "lucas.h"
#include <lucas/Estacao.h>

namespace lucas {
void setup() {
    struct InfoEstacao {
        const char* gcode;
        int pino_botao;
        int pino_led;
    };

    static constexpr std::array<InfoEstacao, Estacao::NUM_ESTACOES> infos = {
        InfoEstacao{ .gcode = RECEITA_PADRAO, .pino_botao = PC8, .pino_led = PE13 },
        InfoEstacao{ .gcode = RECEITA_PADRAO, .pino_botao = PC4, .pino_led = PD13 },
        InfoEstacao{ .gcode = RECEITA_PADRAO, .pino_botao = PA13, .pino_led = PC6 },
        InfoEstacao{ .gcode = RECEITA_PADRAO, .pino_botao = PA4, .pino_led = PE8 },
        InfoEstacao{ .gcode = RECEITA_PADRAO, .pino_botao = PE6, .pino_led = PE15 },
    };

    for (size_t i = 0; i < Estacao::NUM_ESTACOES; i++) {
        auto& info = infos.at(i);
        auto& estacao = Estacao::lista().at(i);

        estacao.set_botao(info.pino_botao);
        estacao.set_led(info.pino_led);
		estacao.set_livre(true);
    }


    DBG("iniciando! - numero de estacoes =", Estacao::NUM_ESTACOES);

	#if LUCAS_CONECTAR_WIFI
		g_conectando_wifi = true;
		DBG("conectando wifi");
		wifi::conectar(LUCAS_WIFI_NOME_SENHA);
	#endif
}

bool pronto();

void pos_execucao_gcode()  {
	if (!pronto())
		return;

	// não prosseguimos com a receita se o bico está ativo
	if (Bico::ativo())
		return;

	#ifdef LUCAS_ROUBAR_FILA_GCODE
		gcode::injetar(LUCAS_ROUBAR_FILA_GCODE);
		return;
	#endif

    auto estacao_ativa = Estacao::ativa();
	if (!estacao_ativa)
		return;

	if (estacao_ativa->aguardando_input()) {
		Estacao::procurar_nova_ativa();
		return;
	} else if (estacao_ativa->livre()) {
		// a receita acabou e o último gcode acabou de ser executado
		estacao_ativa->atualizar_campo_gcode(Estacao::CampoGcode::Atual, "-");
		estacao_ativa->atualizar_status("Livre");
		Estacao::procurar_nova_ativa();
		return;
	} else {
    	estacao_ativa->prosseguir_receita();
	}
}


static void executar_rotina_inicial() {
	DBG("executando rotina inicial");
    gcode::injetar(ROTINA_INICIAL);
	#if LUCAS_DEBUG_GCODE
	    DBG("---- gcode da rotina ----\n", ROTINA_INICIAL);
		DBG("-------------------------");
	#endif
    g_executando_rotina_inicial = true;
}

bool pronto() {
	#if LUCAS_CONECTAR_WIFI
		if (g_conectando_wifi)   {
    	    if (wifi::conectado()) {
    	        g_conectando_wifi = false;
    	        DBG("conectado!");
				DBG("-- informacoeses da rede --");
				DBG("ip = ", wifi::ip().data(),
				" \nnome = ", wifi::nome_rede().data(),
				" \nsenha = ", wifi::senha_rede().data());
				DBG("-------------------------");

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
            DBG("rotina inicial terminada.");
        }
    }

    if (g_trocando_estacao_ativa) {
        if (!gcode::tem_comandos_pendentes()) {
            g_trocando_estacao_ativa = false;
            DBG("troca de estacao finalizada, bico esta na nova posicao");
        }
    }

	return !g_conectando_wifi && !g_executando_rotina_inicial && !g_trocando_estacao_ativa;
}

}
