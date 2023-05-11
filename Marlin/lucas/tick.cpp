#include "tick.h"
#include <lucas/lucas.h>
#include <lucas/Estacao.h>
#include <lucas/util/util.h>
#include <src/module/temperature.h>
#include <src/module/planner.h>

namespace lucas {
static void atualizar_estacoes(millis_t tick) {
    constexpr auto TEMPO_PARA_CANCELAR_RECEITA = 3000; // 3s

    for (auto& estacao : Estacao::lista()) {
        if (estacao.pausada() && estacao.tempo_de_pausa_atingido(tick)) {
            LOG("estacao #", estacao.numero(), " - tempo de pausa acabou");
            estacao.disponibilizar_para_uso();
            continue;
        }

        auto apertado = util::apertado(estacao.botao());
        if (apertado && !estacao.receita().empty()) {
            if (!estacao.tick_apertado_para_cancelar())
                estacao.set_tick_apertado_para_cancelar(tick);

            if (tick - estacao.tick_apertado_para_cancelar() >= TEMPO_PARA_CANCELAR_RECEITA) {
                estacao.cancelar_receita();
                continue;
            }
        }

        // o botão acabou de ser solto
        if (!apertado && estacao.botao_apertado()) {
            if (estacao.tick_apertado_para_cancelar())
                estacao.set_tick_apertado_para_cancelar(0);

            if (estacao.cancelada()) {
                estacao.set_cancelada(false);
            } else {
                if (estacao.livre()) {
                    estacao.set_livre(false);
                    estacao.set_receita(gcode::RECEITA_PADRAO);
                    estacao.aguardar_input();
                } else if (estacao.aguardando_input()) {
                    estacao.disponibilizar_para_uso();
                }
            }
        }

        estacao.set_botao_apertado(apertado);
    }
}

static void atualizar_leds(millis_t tick) {
    constexpr auto TIMER_LEDS = 650; // 650ms
    // é necessario manter um estado geral para que as leds pisquem juntas.
    // poderiamos simplificar essa funcao substituindo o 'WRITE(estacao.led(), ultimo_estado)' por 'TOGGLE(estacao.led())'
    // porém como cada estado dependeria do seu valor individual anterior as leds podem (e vão) sair de sincronia.
    static bool ultimo_estado = true;
    static millis_t ultimo_tick_atualizado = 0;

    for (auto& estacao : Estacao::lista()) {
        if (estacao.esta_ativa() || estacao.disponivel_para_uso()) {
            WRITE(estacao.led(), true);
        } else if (estacao.aguardando_input()) {
            // aguardando input do usuário, led piscando
            WRITE(estacao.led(), ultimo_estado);
        } else {
            // não somos a estacao ativa nem estamos na fila mas estamos disponíveis, led apagada
            WRITE(estacao.led(), false);
        }
    }

    if (tick - ultimo_tick_atualizado >= TIMER_LEDS) {
        ultimo_estado = !ultimo_estado;
        ultimo_tick_atualizado = tick;
    }
}

static void atualizar_serial(millis_t tick) {
    static millis_t ultimo_update_temp = 0;
    if (tick - ultimo_update_temp >= LUCAS_INTERVALO_UPDATE_TEMP) {
        UPDATE(LUCAS_UPDATE_TEMP_ATUAL_BICO, thermalManager.degHotend(0));
        UPDATE(LUCAS_UPDATE_TEMP_TARGET_BICO, thermalManager.degTargetHotend(0));
        UPDATE(LUCAS_UPDATE_TEMP_ATUAL_BOILER, thermalManager.degBed());
        UPDATE(LUCAS_UPDATE_TEMP_TARGET_BOILER, thermalManager.degTargetBed());
        ultimo_update_temp = tick;
    }

    static millis_t ultimo_update_pos = 0;
    if (Estacao::ativa()) {
        if (tick - ultimo_update_pos >= LUCAS_INTERVALO_UPDATE_POS) {
            UPDATE(LUCAS_UPDATE_POSICAO_X, planner.get_axis_position_mm(X_AXIS));
            UPDATE(LUCAS_UPDATE_POSICAO_Y, planner.get_axis_position_mm(Y_AXIS));
            ultimo_update_pos = tick;
        }
    }
}

void tick() {
    if (g_nivelando || wifi::conectando())
        return;

    static millis_t ultimo_tick = 0;
    auto tick = millis();
    Bico::agir(tick);
    //  a 'idle' pode ser chamada mais de uma vez em um milésimo
    //  precisamos fitrar esses casos para nao mudarmos o estado das estacoes/leds mais de uma vez por ms
    if (ultimo_tick == tick)
        return;

    atualizar_leds(tick);
    atualizar_estacoes(tick);
    atualizar_serial(tick);

    ultimo_tick = tick;
}
}
