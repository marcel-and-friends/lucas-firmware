#include "kofy.h"
#include "Boca.h"
#include <src/MarlinCore.h>
#include <src/module/temperature.h>

namespace kofy {

// Gcode executado ao apertar os botões
// (lembrando que as coordenadas são relativas!)
static constexpr auto GCODE_CAFE = 
R"(G4 P5000
G0 F2000 X10
G0 F2000 X-10
G0 F2000 X0
K0
G0 F1000 E50
G4 P3000
G0 F1000 E0)";
// K0: Interrompe a boca até o botão ser apertado

// Gcode necessário para o funcionamento ideal da máquina, executando quando a máquina liga, logo após conectar ao WiFi
static constexpr auto ROTINA_INICIAL =
R"(G28 X Y
M190 R83)"; // MUDAR PARA R93, aqui é a temperatura ideal, a máquina não fará nada até chegar nesse ponto

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

    
    DBG("bocas inicializadas (", Boca::NUM_BOCAS, ").");
    DBG("todas as bocas começam indisponíveis, aperte um dos botões para iniciar uma receita.");

	marlin::wifi::conectar("VIVOFIBRA-CASA4", "kira243casa4");

    g_conectando_wifi = true;
}

    
void idle() {
    static uint64_t ultimo_tick = 0;
    auto tick = millis();

	for (auto& boca : Boca::lista()) {
        if (boca.aguardando_botao()) {
            auto timer = boca.progresso_receita() ? 250 : 500;
            // a variável "ultimo_tick" é necessária pois a 'idle()' do marlin
            // é chamada mais de uma vez por tick
            if (tick % timer == 0 && ultimo_tick != tick) {
                TOGGLE(boca.led());
            }
        } else {
            WRITE(boca.led(), Boca::boca_ativa() == &boca);
        }
    }

    ultimo_tick = tick;
    if (g_inicializando)
        return;

    for (auto& boca : Boca::lista()) {          
		if (!boca.aguardando_botao())
			continue;
		
		if (Boca::boca_ativa() && Boca::boca_ativa() == &boca)
			continue;

		if (marlin::apertado(boca.botao())) {
			boca.disponibilizar_para_uso();
			if (!Boca::boca_ativa()) 
				Boca::set_boca_ativa(&boca);
        }
    }
}

void event_handler()  {
    if (g_conectando_wifi)   {
        if (marlin::wifi::conectado()) {
            g_conectando_wifi = false;
            DBG("conectado\nip = ", marlin::wifi::ip().data(),
			" \nnome = ", marlin::wifi::nome_rede().data(),
			" \nsenha = ", marlin::wifi::senha_rede().data());

			#if KOFY_CUIDAR_TEMP
			DBG("iniciando rotina inicial");
            marlin::injetar_gcode(ROTINA_INICIAL);
            g_inicializando = true;
			#endif
        }
        return;
    }

    if (g_inicializando) {
        if (!queue.injected_commands_P) {
            g_inicializando = false;
            DBG("rotina inicial terminada.");
        }
        return;
    }

    if (g_mudando_boca_ativa) {
        if (!queue.injected_commands_P) {
            g_mudando_boca_ativa = false;
            DBG("bico chegou na posição nova");
        }
        return;
    }

    auto boca_ativa = Boca::boca_ativa();
    if (!boca_ativa) {
        return;
	}

	if (boca_ativa->aguardando_botao()) {
		Boca::procurar_nova_boca_ativa();
		return;
	}

    if (boca_ativa->proxima_instrucao() == "K0") {
        DBG("executando 'K0' na boca #", boca_ativa->numero(), " - pressione o botão para finalizar a receita.");

        boca_ativa->pular_proxima_instrucao();
        boca_ativa->aguardar_botao();
		Boca::procurar_nova_boca_ativa();
    } else {
        boca_ativa->progredir_receita();
    }
}

}
