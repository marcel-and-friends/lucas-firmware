#include "lucas.h"
#include <lucas/Boca.h>
#include <lucas/util/util.h>

namespace lucas {

// Gcode executado ao apertar os botões
// (lembrando que as coordenadas são relativas!)
static constexpr auto GCODE_CAFE =
R"(G3 F5000 I20 J20
G3 F5000 I15 J15
G3 F5000 I10 J10
G3 F5000 I5 J5
K0
G3 F5000 I5 J5
G3 F5000 I10 J10
G3 F5000 I15 J15
G3 F5000 I20 J20)";

// Gcodes necessário para o funcionamento ideal da máquina, executando quando a máquina liga, logo após conectar ao WiFi
static constexpr auto ROTINA_INICIAL =
R"(G28 X Y
M190 R93)";

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

	#if LUCAS_CONECTAR_WIFI
		g_conectando_wifi = true;
		DBG("conectando wifi");
		// wifi::conectar("VIVOFIBRA-CASA4", "kira243casa4"); // marcio
		// wifi::conectar("CLARO_2G97D2E8", "1297D2E8"); // vini
		wifi::conectar("Kika-Amora", "Desconto5"); // marcel
	#endif
}

void atualizar_leds(millis_t tick);
void atualizar_botoes(millis_t tick);

void idle() {
    if (g_inicializando || g_conectando_wifi)
        return;

	// a 'idle' pode ser chamada mais de uma vez em um milésimo, precisamos fitrar esses casos
	// OBS: essa variável pode ser inicializada com 'millis()' também mas daí perderíamos o primeiro tick...
    static millis_t ultimo_estado_tick = 0;
    auto tick = millis();
	if (ultimo_estado_tick == tick)
		return;

	atualizar_leds(tick);

	if (Bico::ativo())
		Bico::agir(tick);

	atualizar_botoes(tick);

    ultimo_estado_tick = tick;
}

bool pronto();

void event_handler()  {
	if (!pronto())
		return;

	// não prosseguimos com a receita se o bico está ativo
	if (Bico::ativo())
		return;

	#ifdef LUCAS_ROUBAR_FILA_GCODE
		gcode::injetar(LUCAS_ROUBAR_FILA_GCODE);
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
	// é necessario manter um estado geral para que as leds se mantenham em sincronia.
	// poderiamos simplificar essa funcao substituindo o 'WRITE(boca.led(), ultimo_estado)' por 'TOGGLE(boca.led())'
	// porém como cada estado dependeria do seu valor individual anterior as leds podem (e vão) sair de sincronia.
	static bool ultimo_estado = true;

	for (auto& boca : Boca::lista()) {
		if (Boca::ativa() == &boca) {
			// somos a boca ativa, led ligada em cor sólida
            WRITE(boca.led(), true);
		} else if (boca.aguardando_botao()) {
			// aguardando input do usuário, led piscando
            if (tick % TIMER_LED == 0)
                WRITE(boca.led(), ultimo_estado);
		} else {
			// não somos a boca ativa mas estamos disponíveis, led apagada
            WRITE(boca.led(), false);
		}
    }

    if (tick % TIMER_LED == 0)
		ultimo_estado = !ultimo_estado;
}

void atualizar_botoes(millis_t tick) {
	constexpr auto TEMPO_PARA_CANCELAR_RECEITA = 3000; // 3s
	static int botao_boca_cancelada = 0;
	if (Boca::ativa()) {
		auto& boca = *Boca::ativa();
		if (util::apertado(boca.botao())) {
			if (!boca.tick_apertado_para_cancelar()) {
				boca.set_tick_apertado_para_cancelar(tick);
				return;
			}

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

	// se ainda está apertado o botão da boca que acabou de ser cancelada
	// nós não atualizamos a boca ativa e/ou disponibilizamos nova bocas para uso
	// sem essa verificação a boca cancelada é instantaneamente marcada como disponível após seu cancelamento
	if (botao_boca_cancelada && util::apertado(botao_boca_cancelada))
		return;

	// quando o botão é solto podemos reiniciar essa variável
	botao_boca_cancelada = 0;

    for (auto& boca : Boca::lista()) {
		auto apertado = util::apertado(boca.botao());
		// o botão acabou de ser solto...
		if (!apertado && boca.botao_apertado() && boca.aguardando_botao()) {
			if (Boca::ativa() && Boca::ativa() == &boca)
				continue;

			boca.disponibilizar_para_uso();
			if (!Boca::ativa())
				Boca::set_boca_ativa(&boca);
        }

		boca.set_botao_apertado(apertado);
    }
}

bool pronto() {
	if (g_conectando_wifi)   {
        if (wifi::conectado()) {
            g_conectando_wifi = false;
            DBG("conectado!");
			DBG("-- informações da rede --");
			DBG("ip = ", wifi::ip().data(),
			" \nnome = ", wifi::nome_rede().data(),
			" \nsenha = ", wifi::senha_rede().data());
			DBG("-------------------------");

			#if LUCAS_ROTINA_INICIAL
				DBG("executando rotina inicial");
            	gcode::injetar(ROTINA_INICIAL);
				#if LUCAS_DEBUG_GCODE
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
