#include "kofy.h"
#include "Boca.h"
#include <src/MarlinCore.h>
#include <src/module/temperature.h>
#include <src/lcd/extui/mks_ui/draw_ui.h>

namespace kofy {

static constexpr auto GCODE_CAFE = 
R"(M302 S0
K0
G1 F150 X5)";

// Aqui se coloca todos os gcode necessário para o funcionamento ideal da máquina
static constexpr auto ROTINA_INICIAL =
R"(M302 S0
G28 X Y
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


	constexpr std::string_view nome = "Cafe2_4";
	constexpr std::string_view senha = "Lobobobo";
	marlin::conectar_wifi(nome, senha);
    g_conectando_wifi = true;

    DBG("bocas inicializadas (", Boca::NUM_BOCAS, ").");

    DBG("todas as bocas começam indisponíveis, aperte um dos botões para iniciar uma receita.");
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
    if (g_inicializando || g_mudando_boca_ativa)
        return;

    for (auto& boca : Boca::lista()) {          
            if (marlin::apertado(boca.botao())) {
                boca.disponibilizar_para_uso();
                if (!Boca::boca_ativa())
                    Boca::set_boca_ativa(&boca);
            
        }
    }
}

void event_handler()  {
    if (g_conectando_wifi)   {
        if (marlin::conectado_a_wifi()) {
            g_conectando_wifi = false;
            DBG("conectado!");

            marlin::injetar_gcode(ROTINA_INICIAL);
            g_inicializando = true;
        }
        return;
    }

    if (g_inicializando) {
        if (!queue.injected_commands_P) {
            g_inicializando = false;
            DBG("inicialização terminada!");
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
    if (!boca_ativa)
        return;

    if (boca_ativa->proxima_instrucao() == "K0") {
        DBG("executando 'K0' na boca #", boca_ativa->numero(), " - pressione o botão para disponibilizar a boca.");

        boca_ativa->pular_proxima_instrucao();
        boca_ativa->aguardar_botao();
        Boca::procurar_nova_boca_ativa();
                
        marlin::parar_fila_de_gcode();
    } else {
        boca_ativa->progredir_receita();
    }
}

namespace marlin {

void conectar_wifi(std::string_view nome_rede, std::string_view senha_rede) {
    memcpy(ipPara.ip_addr, "0.0.0.0", sizeof("0.0.0.0"));

	ZERO(uiCfg.wifi_name);
	memcpy(uiCfg.wifi_name, nome_rede.data(), nome_rede.length() + 1);

	ZERO(uiCfg.wifi_key);
	memcpy(uiCfg.wifi_key, senha_rede.data(), senha_rede.length() + 1);

	gCfgItems.wifi_mode_sel = STA_MODEL;
	package_to_wifi(WIFI_PARA_SET, nullptr, 0);

	std::array<uint8_t, 6> magica = { 0xA5, 0x09, 0x01, 0x00, 0x01, 0xFC };
    raw_send_to_wifi(magica.data(), magica.size());

	DBG("conectando wifi...");
}

bool conectado_a_wifi() {
    return strstr(ipPara.ip_addr, "0.0.0.0") == nullptr;
}

}

}