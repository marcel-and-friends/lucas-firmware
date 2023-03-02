#include "kofy.h"
#include "Boca.h"
#include <src/MarlinCore.h>

namespace kofy {

void setup() {
    static constexpr auto GCODE_CAFE = 
R"(M302 P0 S0
G0 F8000 X620 Y35; LEVA O BICO NA POSIÇÃO
G4 P1000; TEMPO 
G1 F800; AVANÇO MENOR
G3 I0 J5; RAIO PEQUENO
G1 F800 Y25
G3 I0 J10;RAIO MEDIO1
G1 F800 Y30
G3 I0 J15;RAIO MEDIO2
G1 F800 Y35
G3 I0 J20;RAIO MEDIO3
G1 F800 Y5
G3 I0 J25;RAIO MEDIO3
G1 F800 Y35
G0 F8000 X0 Y0;
M0
G0 F8000 X620 Y35; LEVA O BICO NA POSIÇÃO
G4 P1000; TEMPO 
G1 F800; AVANÇO MENOR
G3 I0 J5; RAIO PEQUENO
G1 F800 Y25
G3 I0 J10;RAIO MEDIO1
G1 F800 Y30
G3 I0 J15;RAIO MEDIO2
G1 F800 Y35
G3 I0 J20;RAIO MEDIO3
G1 F800 Y5
G3 I0 J25;RAIO MEDIO3
G1 F800 Y35
G0 F8000 X0 Y0;)";

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
    auto boca_ativa = Boca::boca_ativa();
    if (!boca_ativa)
        return;

    // na nossa máquina o 'M0' serve apenas como um sinalizador de que
    // o usuário tem que apertar o botão da boca para prosseguir com a receita.
    // ele nunca é inserido na fila de gcode do marlin.
    if (boca_ativa->proxima_instrucao() == "M0") {
        DBG("pulando 'M0' da boca #", boca_ativa->numero(), " - pressione o botão para disponibilizar a boca.");

        boca_ativa->pular_proxima_instrucao();
        boca_ativa->aguardar_botao();
        Boca::procurar_nova_boca_ativa();
                
        marlin::parar_fila_de_gcode();
    } else {
        boca_ativa->progredir_receita();
    }
}

}