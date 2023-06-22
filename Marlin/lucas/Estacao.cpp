#include "Estacao.h"
#include <memory>
#include <src/module/temperature.h>
#include <lucas/Bico.h>
#include <lucas/Fila.h>
#include <lucas/cmd/cmd.h>

#define ESTACAO_LOG(...) LOG("ESTACAO #", this->numero(), ": ", "", __VA_ARGS__)

namespace lucas {
Estacao::Lista Estacao::s_lista = {};

void Estacao::iniciar(size_t num) {
    if (num > NUM_MAX_ESTACOES) {
        LOG("numero de estacoes invalido - [max = ", NUM_MAX_ESTACOES, "]");
        return;
    }

    if (s_num_estacoes) {
        LOG("estacoes ja iniciadas");
        return;
    }

    struct InfoEstacao {
        int pino_botao;
        int pino_led;
    };

    static constexpr std::array<InfoEstacao, Estacao::NUM_MAX_ESTACOES> infos = {
        InfoEstacao{.pino_botao = PC8,  .pino_led = PE13},
        InfoEstacao{ .pino_botao = PC4, .pino_led = PD13}
    };

    for (size_t i = 0; i < num; i++) {
        auto& info = infos.at(i);
        auto& estacao = Estacao::lista().at(i);
        estacao.set_botao(info.pino_botao);
        estacao.set_led(info.pino_led);
        // todas as maquinas começam livres
        estacao.set_status(Status::FREE);
    }

    s_num_estacoes = num;

    LOG("maquina vai usar ", num, " estacoes");
}

void Estacao::tick() {
    // atualizacao dos estados das estacoes
    {
        for_each([](Estacao& estacao) {
            const auto tick = millis();
            // absolutamente nada é atualizado se a estacao estiver bloqueada
            if (estacao.bloqueada())
                return util::Iter::Continue;

            // atualizacao simples do estado do botao
            bool segurado_antes = estacao.botao_segurado();
            bool segurado_agora = util::segurando(estacao.botao());
            estacao.set_botao_segurado(segurado_agora);

            // agora vemos se o usuario quer cancelar a receita
            {
                constexpr auto TEMPO_PARA_CANCELAR_RECEITA = 3000; // 3s
                if (segurado_agora) {
                    if (!estacao.tick_botao_segurado())
                        estacao.set_tick_botao_segurado(tick);

                    if (tick - estacao.tick_botao_segurado() >= TEMPO_PARA_CANCELAR_RECEITA) {
                        Fila::the().cancelar_receita(estacao.index());
                        estacao.set_receita_cancelada(true);
                        return util::Iter::Continue;
                    }
                }
            }

            // o botão acabou de ser solto, temos varias opcoes
            if (!segurado_agora && segurado_antes) {
                if (estacao.tick_botao_segurado()) {
                    // logicamente o botao ja nao está mais sendo segurado (pois acabou de ser solto)
                    estacao.set_tick_botao_segurado(0);
                }

                if (estacao.receita_cancelada()) {
                    // se a receita acabou de ser cancelada, podemos voltar ao normal
                    // o proposito disso é não enviar a receita padrao imediatamente após cancelar uma receita
                    estacao.set_receita_cancelada(false);
                    return util::Iter::Continue;
                }

                switch (estacao.status()) {
                // TODO: TEM QUE COMUNICAR PRO APP QUE ISSO AQUI ACONTECEU
                case Status::FREE:
                    // isso aqui é so qnd nao tiver conectado no app
                    Fila::the().agendar_receita(estacao.index(), Receita::padrao());
                    break;
                case Status::WAITING_START:
                case Status::INITIALIZE_COFFEE:
                    Fila::the().mapear_receita(estacao.index());
                    break;
                case Status::IS_READY:
                    estacao.set_status(Status::FREE);
                    break;
                default:
                    break;
                }
            }

            return util::Iter::Continue;
        });
    }

    // atualizacao das leds
    {
        constexpr auto TIMER_LEDS = 500;
        // é necessario manter um estado geral para que as leds pisquem juntas.
        // poderiamos simplificar essa funcao substituindo o 'WRITE(estacao.led(), ultimo_estado)' por 'TOGGLE(estacao.led())'
        // porém como cada estado dependeria do seu valor individual anterior as leds podem (e vão) sair de sincronia.
        static bool ultimo_estado = true;
        static millis_t ultimo_tick_atualizado = 0;
        const auto tick = millis();
        if (tick - ultimo_tick_atualizado >= TIMER_LEDS) {
            ultimo_estado = !ultimo_estado;
            ultimo_tick_atualizado = tick;
            for_each_if(&Estacao::aguardando_input, [](const Estacao& estacao) {
                // aguardando input do usuário - led piscando
                WRITE(estacao.led(), ultimo_estado);
                return util::Iter::Continue;
            });
        }
    }
}

float Estacao::posicao_absoluta(size_t index) {
    auto distancia_primeira_estacao = 80.f / util::step_ratio();
    auto distancia_entre_estacoes = 160.f / util::step_ratio();
    return distancia_primeira_estacao + index * distancia_entre_estacoes;
}

size_t Estacao::numero() const {
    return index() + 1;
}

Estacao::Index Estacao::index() const {
    // cute
    return ((uintptr_t)this - (uintptr_t)&s_lista) / sizeof(Estacao);
}

void Estacao::set_led(pin_t pino) {
    m_pino_led = pino;
    SET_OUTPUT(m_pino_led);
    WRITE(m_pino_led, LOW);
}

void Estacao::set_botao(pin_t pino) {
    m_pino_botao = pino;
    SET_INPUT_PULLUP(m_pino_botao);
    WRITE(m_pino_botao, HIGH);
}

void Estacao::set_bloqueada(bool b) {
    auto antigo = m_bloqueada;
    m_bloqueada = b;
    if (antigo != m_bloqueada)
        ESTACAO_LOG(m_bloqueada ? "" : "des", "bloqueada");
}

void Estacao::set_status(Status status) {
    m_status = status;
    switch (m_status) {
    case Status::FREE:
        WRITE(m_pino_led, LOW);
        break;
    case Status::SCALDING:
    case Status::MAKING_COFFEE:
    case Status::NOTIFICATION_TIME:
        WRITE(m_pino_led, HIGH);
        break;
    default:
        break;
    }
}
}
