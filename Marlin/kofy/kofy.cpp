#include "kofy.h"
#include "Boca.h"
#include <src/MarlinCore.h>

namespace kofy {

static constexpr auto GCODE_CAFE = 
R"(M302 S0
K0
G1 F150 X5)";

// Aqui se coloca todos os gcode necessário para o funcionamento ideal da máquina
static constexpr auto ROTINA_INICIAL =
R"(M302 S0
G28 X Y)";

void setup() {
    struct InfoBoca {
        const char* gcode;
        int pino_botao;
        int pino_led;
    };

    static constexpr std::array<InfoBoca, Boca::NUM_BOCAS> infos = {
        InfoBoca{ GCODE_CAFE, Z_MIN_PIN, PE13 },
        InfoBoca{ GCODE_CAFE, Z_MAX_PIN, PD13 },
        InfoBoca{ GCODE_CAFE, PA13, PC6 },
        InfoBoca{ GCODE_CAFE, PA4, PE8 },
        InfoBoca{ GCODE_CAFE, PE6, PE15 },
    };

    for (size_t i = 0; i < Boca::NUM_BOCAS; i++) {
        auto& info = infos.at(i);
        auto& boca = Boca::lista().at(i);

        boca.set_receita(info.gcode);
        boca.set_botao(info.pino_botao);
        boca.set_led(info.pino_led);
    }

    marlin::injetar_gcode(ROTINA_INICIAL);
    g_inicializando = true;

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
            
            if (marlin::apertado(boca.botao())) {
                boca.disponibilizar_para_uso();
                if (!Boca::boca_ativa())
                    Boca::set_boca_ativa(&boca);
            }
            
        } else {
            WRITE(boca.led(), Boca::boca_ativa() == &boca);
        }
    }

    ultimo_tick = tick;
}

void event_handler()  {
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

}