#include "kofy.h"
#include "Boca.h"
#include <src/module/temperature.h>
#include <src/gcode/parser.h>

namespace kofy {

// Gcode executado ao apertar os botões
// (lembrando que as coordenadas são relativas!)
static constexpr auto GCODE_CAFE =
R"(G3 F5000 I20 J20 P5
K0
G3 F5000 I20 J20 P5)";

// Gcode necessário para o funcionamento ideal da máquina, executando quando a máquina liga, logo após conectar ao WiFi
static constexpr auto ROTINA_INICIAL =
R"(G28 X Y
M190 R93)"; // MUDAR PARA R93, aqui é a temperatura ideal, a máquina não fará nada até chegar nesse ponto

void setup() {
    struct InfoBoca {
        const char* gcode;
        int pino_botao;
        int pino_led;
    };

    static constexpr std::array<InfoBoca, Boca::NUM_BOCAS> infos = {
        InfoBoca{ .gcode = GCODE_CAFE, .pino_botao = PC8, .pino_led = PE13 },
        InfoBoca{ .gcode = GCODE_CAFE, .pino_botao = PC4, .pino_led = PD13 },
        InfoBoca{ .gcode = GCODE_CAFE, .pino_botao = PA13, .pino_led = PC6 },
        InfoBoca{ .gcode = GCODE_CAFE, .pino_botao = PA4, .pino_led = PE8 },
        InfoBoca{ .gcode = GCODE_CAFE, .pino_botao = PE6, .pino_led = PE15 },
    };

    for (size_t i = 0; i < Boca::NUM_BOCAS; i++) {
        auto& info = infos.at(i);
        auto& boca = Boca::lista().at(i);

        boca.set_receita(info.gcode);
        boca.set_botao(info.pino_botao);
        boca.set_led(info.pino_led);
    }


    DBG("bocas inicializadas - ", Boca::NUM_BOCAS);
    DBG("todas as bocas começam indisponíveis, aperte um dos botões para iniciar uma receita");

    g_conectando_wifi = true;
	DBG("conectando wifi");
	// marlin::wifi::conectar("VIVOFIBRA-CASA4", "kira243casa4"); // marcio
	// marlin::wifi::conectar("CLARO_2G97D2E8", "1297D2E8"); // vini
	marlin::wifi::conectar("Kika-Amora", "Desconto5"); // marcel

}

void atualizar_leds(millis_t tick);
void atualizar_botoes(millis_t tick);

void idle() {
    if (g_inicializando || g_conectando_wifi)
        return;

	// a 'idle' pode ser chamada mais de uma vez em um milésimo, precisamos fitrar esses casos
	// OBS: essa variável pode ser inicializada com 'millis()' também mas daí perderíamos o primeiro tick...
    static millis_t ultimo_tick = 0;
    auto tick = millis();
	if (ultimo_tick == tick)
		return;

	atualizar_leds(tick);

	if (Bico::ativo())
		Bico::agir(tick);

	atualizar_botoes(tick);

    ultimo_tick = tick;
}

bool pronto();

void event_handler()  {
	if (!pronto())
		return;

	// não prosseguimos com a receita se o bico está ativo
	if (Bico::ativo())
		return;

	#ifdef KOFY_ROUBAR_FILA_GCODE
		gcode::injetar(KOFY_ROUBAR_FILA_GCODE);
		return;
	#endif

    auto boca_ativa = Boca::ativa();
	if (!boca_ativa)
		return;

	if (boca_ativa->aguardando_botao()) {
		Boca::procurar_nova_boca_ativa();
		return;
	}

    boca_ativa->prosseguir_receita();
}

void atualizar_leds(millis_t tick) {
	constexpr auto TIMER_LED = 650; // 650ms
	static bool ultimo = true;

	for (auto& boca : Boca::lista()) {
        if (boca.aguardando_botao()) {
			// bocas indisponíveis piscam em um intervalo constante
			// FIXME: essa lógica tem que mudar quando chegar o aplicativo
            if (tick % TIMER_LED == 0) {
                WRITE(boca.led(), ultimo);
			}
        } else {
			// a boca está disponível...
			// 		- se está ativa fica ligada estaticamente
			// 		- se não está ativa fica apagada
            WRITE(boca.led(), Boca::ativa() == &boca);
        }
    }

    if (tick % TIMER_LED == 0)
		ultimo = !ultimo;
}

void atualizar_botoes(millis_t tick) {
	constexpr auto TEMPO_PARA_CANCELAR_RECEITA = 3000; // 3s
	static int botao_boca_cancelada = 0;
	if (Boca::ativa()) {
		auto& boca = *Boca::ativa();
		if (marlin::apertado(boca.botao())) {
			if (!boca.tick_apertado_para_cancelar())
				boca.set_tick_apertado_para_cancelar(tick);

			if (tick - boca.tick_apertado_para_cancelar() >= TEMPO_PARA_CANCELAR_RECEITA)	 {
				boca.cancelar_receita();
				botao_boca_cancelada = boca.botao();
				DBG("!!!! RECEITA CANCELADA !!!!");
				return;
			}
		} else {
			botao_boca_cancelada = 0;
			boca.set_tick_apertado_para_cancelar(0);
		}
	}

	if (botao_boca_cancelada && marlin::apertado(botao_boca_cancelada))
		return;

	botao_boca_cancelada = 0;

    for (auto& boca : Boca::lista()) {
		auto apertado = marlin::apertado(boca.botao());
		if (!apertado && boca.botao_apertado() && boca.aguardando_botao()) {
			// o botão acabou de ser solto
			// se a boca já é a ativa não é necessário fazer nada
			if (Boca::ativa() && Boca::ativa() == &boca)
				continue;

			boca.disponibilizar_para_uso();
			// se ainda não temos uma boca ativa setamos essa
			if (!Boca::ativa())
				Boca::set_boca_ativa(&boca);
        }

		boca.set_botao_apertado(apertado);
    }
}

bool pronto() {
	if (g_conectando_wifi)   {
        if (marlin::wifi::conectado()) {
            g_conectando_wifi = false;
            DBG("conectado!");
			DBG("-- informações da rede --");
			DBG("ip = ", marlin::wifi::ip().data(),
			" \nnome = ", marlin::wifi::nome_rede().data(),
			" \nsenha = ", marlin::wifi::senha_rede().data());
			DBG("-------------------------");

			#if KOFY_ROTINA_INICIAL
				DBG("executando rotina inicial");
            	gcode::injetar(ROTINA_INICIAL);
				#if KOFY_DEBUG_GCODE
				    DBG("---- gcode da rotina ----\n", ROTINA_INICIAL);
					DBG("-------------------------");
				#endif
            	g_inicializando = true;
			#endif
        }
    }

    if (g_inicializando) {
        if (!gcode::tem_comandos_pendentes()) {
            g_inicializando = false;
            DBG("rotina inicial terminada.");
        }
    }

    if (g_mudando_boca_ativa) {
        if (!gcode::tem_comandos_pendentes()) {
            g_mudando_boca_ativa = false;
            DBG("troca de boca finalizada, bico está na nova posição");
        }
    }

	return !g_conectando_wifi && !g_inicializando && !g_mudando_boca_ativa;
}

}
