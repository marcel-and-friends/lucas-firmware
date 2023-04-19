#include "tick.h"
#include <lucas/lucas.h>
#include <lucas/Estacao.h>
#include <lucas/util/util.h>
#include <src/module/temperature.h>
#include <src/module/planner.h>

namespace lucas {
static void atualizar_leds(millis_t tick);
static void atualizar_estacoes(millis_t tick);
static void atualizar_serial(millis_t diff);

void tick() {
    if (g_executando_rotina_inicial || g_conectando_wifi)
        return;

    static millis_t ultimo_tick = 0;
    auto tick = millis();

    Bico::agir(tick);
    // a 'idle' pode ser chamada mais de uma vez em um milésimo
    // precisamos fitrar esses casos para nao mudarmos o estado das estacoes/leds mais de uma vez por ms
    if (ultimo_tick == tick)
        return;

    atualizar_serial(tick);
    atualizar_leds(tick);
    atualizar_estacoes(tick);

    ultimo_tick = tick;
}

static void atualizar_leds(millis_t tick) {
    constexpr auto TIMER_LEDS = 650; // 650ms
    // é necessario manter um estado geral para que as leds pisquem juntas.
    // poderiamos simplificar essa funcao substituindo o 'WRITE(estacao.led(), ultimo_estado)' por 'TOGGLE(estacao.led())'
    // porém como cada estado dependeria do seu valor individual anterior as leds podem (e vão) sair de sincronia.
    static bool ultimo_estado = true;
    static millis_t ultimo_tick_atualizado = 0;

    for (auto& estacao : Estacao::lista()) {
        if (estacao.esta_ativa() || estacao.esta_na_fila()) {
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

static bool tentar_cancelar_receita(millis_t tick) {
    constexpr auto TEMPO_PARA_CANCELAR_RECEITA = 3000; // 3s
    static int botao_estacao_cancelada = 0;

    if (!Estacao::ativa())
        return false;

    auto& estacao = *Estacao::ativa();
    if (util::apertado(estacao.botao())) {
        if (!estacao.tick_apertado_para_cancelar()) {
            estacao.set_tick_apertado_para_cancelar(tick);
            return false;
        }

        if (tick - estacao.tick_apertado_para_cancelar() >= TEMPO_PARA_CANCELAR_RECEITA) {
            estacao.cancelar_receita();
            botao_estacao_cancelada = estacao.botao();
            DBG("!!!! RECEITA CANCELADA !!!!");
            return true;
        }
    } else {
        botao_estacao_cancelada = 0;
        estacao.set_tick_apertado_para_cancelar(0);
    }

    // se ainda está apertado o botão da estacao que acabou de ser cancelada
    // nós não atualizamos a estacao ativa e/ou disponibilizamos nova estacoes para uso
    // sem essa verificação a estacao cancelada é instantaneamente marcada como disponível após seu cancelamento
    if (botao_estacao_cancelada && util::apertado(botao_estacao_cancelada))
        return true;

    // quando o botão é solto podemos reiniciar essa variável
    botao_estacao_cancelada = 0;

    return false;
}

static void atualizar_estacoes(millis_t tick) {
    if (tentar_cancelar_receita(tick))
        return;

    for (auto& estacao : Estacao::lista()) {
        auto apertado = util::apertado(estacao.botao());
        if (!apertado && estacao.botao_apertado()) {
            // o botão acabou de ser solto...
            if (estacao.livre()) {
                estacao.set_receita(RECEITA_PADRAO);
                estacao.set_livre(false);
                estacao.aguardar_input();
            } else if (estacao.aguardando_input()) {
                estacao.disponibilizar_para_uso();
                if (!Estacao::ativa())
                    Estacao::set_estacao_ativa(&estacao);
            }
        }
        estacao.set_botao_apertado(apertado);
    }
}

static void atualizar_serial(millis_t tick) {
    static millis_t ultimo_update_temp = 0;
    if (tick - ultimo_update_temp >= LUCAS_INTERVALO_UPDATE_TEMP) {
        UPDATE(LUCAS_NOME_UPDATE_TEMP_ATUAL_BICO, thermalManager.degHotend(0));
        UPDATE(LUCAS_NOME_UPDATE_TEMP_TARGET_BICO, thermalManager.degTargetHotend(0));
        UPDATE(LUCAS_NOME_UPDATE_TEMP_ATUAL_BOILER, thermalManager.degBed());
        UPDATE(LUCAS_NOME_UPDATE_TEMP_TARGET_BOILER, thermalManager.degTargetBed());
        ultimo_update_temp = tick;
    }

    static millis_t ultimo_update_pos = 0;
    if (Estacao::ativa()) {
        if (tick - ultimo_update_pos >= LUCAS_INTERVALO_UPDATE_POS) {
            UPDATE(LUCAS_NOME_UPDATE_POSICAO_X, planner.get_axis_position_mm(X_AXIS));
            UPDATE(LUCAS_NOME_UPDATE_POSICAO_Y, planner.get_axis_position_mm(Y_AXIS));
            ultimo_update_pos = tick;
        }
    }
}
}
