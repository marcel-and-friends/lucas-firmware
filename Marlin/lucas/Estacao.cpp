#include "Estacao.h"
#include <memory>
#include <src/module/temperature.h>
#include <lucas/Bico.h>
#include <lucas/Fila.h>
#include <lucas/gcode/gcode.h>

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
        estacao.set_livre(true);
    }

    s_num_estacoes = num;

    LOG("maquina vai usar ", num, " estacoes");
}

void Estacao::tick(millis_t tick) {
    // atualizacao dos estados das estacoes
    {
        for_each([tick](auto& estacao) {
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

                if (estacao.livre()) {
                    // se a boca estava livre e apertamos o botao enviamos a receita padrao
                    Fila::the().agendar_receita(estacao.index(), Receita::padrao());
                    return util::Iter::Continue;
                }

                if (estacao.aguardando_input()) {
                    // se estavamos aguardando input prosseguimos com a receita
                    Fila::the().mapear_receita(estacao.index());
                    return util::Iter::Continue;
                }
            }

            return util::Iter::Continue;
        });
    }

    // atualizacao das leds
    {
        constexpr auto TIMER_LEDS = 650; // 650ms
        // é necessario manter um estado geral para que as leds pisquem juntas.
        // poderiamos simplificar essa funcao substituindo o 'WRITE(estacao.led(), ultimo_estado)' por 'TOGGLE(estacao.led())'
        // porém como cada estado dependeria do seu valor individual anterior as leds podem (e vão) sair de sincronia.
        static bool ultimo_estado = true;
        static millis_t ultimo_tick_atualizado = 0;

        for_each([tick](const auto& estacao) {
            if (estacao.esta_ativa() || estacao.disponivel_para_uso()) {
                WRITE(estacao.led(), true);
            } else if (estacao.aguardando_input()) {
                // aguardando input do usuário - led piscando
                WRITE(estacao.led(), ultimo_estado);
            } else {
                // não somos a estacao ativa nem estamos na fila - led apagada
                WRITE(estacao.led(), false);
            }
            return util::Iter::Continue;
        });

        if (tick - ultimo_tick_atualizado >= TIMER_LEDS) {
            ultimo_estado = !ultimo_estado;
            ultimo_tick_atualizado = tick;
        }
    }
}

// TODO: desenvolver um algoritmo legal pra isso...
void Estacao::procurar_nova_ativa() {
    LOG("procurando nova estacao ativa...");

    bool achou = false;
    for_each([&achou](auto& estacao) {
        if (estacao.disponivel_para_uso()) {
            achou = true;
            set_estacao_ativa(&estacao);
            return util::Iter::Stop;
        }
        return util::Iter::Continue;
    });

    if (achou)
        return;

    LOG("nenhuma estacao esta disponivel :(");
    set_estacao_ativa(nullptr);
}

void Estacao::set_estacao_ativa(Estacao* estacao) {
    if (estacao == s_estacao_ativa)
        return;

    s_estacao_ativa = estacao;
    if (s_estacao_ativa) {
        auto& estacao = *s_estacao_ativa;
        estacao.set_status(Status::MAKING_COFFEE);

        auto& bico = Bico::the();
        // bico.descartar_agua_ruim();
        bico.viajar_para_estacao(estacao);

        LOG("ESTACAO #", estacao.numero(), ": escolhida como nova estacao ativa");
    } else {
        cmd::parar_fila();
    }
}

float Estacao::posicao_absoluta(size_t index) {
    auto distancia_primeira_estacao = 80.f / util::step_ratio();
    auto distancia_entre_estacoes = 160.f / util::step_ratio();
    return distancia_primeira_estacao + index * distancia_entre_estacoes;
}

void Estacao::receber_receita(JsonObjectConst json) {
    auto maybe_receita = Receita::from_json(json);
    // set_receita(receita);
    set_livre(false);
    set_aguardando_input(true);
}

void Estacao::prosseguir_receita() {
    // auto instrucao = m_receita.proxima_instrucao();
    // atualizar_campo_gcode(CampoGcode::Atual, instrucao);
    // if (cmd::ultima_instrucao(instrucao.data())) {
    //     atualizar_campo_gcode(CampoGcode::Proximo, "-");
    //     // podemos 'executar()' aqui pois, como é a ultima instrucao, a string é null-terminated
    //     cmd::executar(instrucao.data());
    //     reiniciar();
    //     ESTACAO_LOG("receita finalizada");
    //     atualizar_campo_gcode(CampoGcode::Atual, "-");
    // } else {
    //     m_receita.prosseguir();
    //     atualizar_campo_gcode(CampoGcode::Proximo, m_receita.proxima_instrucao());
    // }
}

void Estacao::disponibilizar_para_uso() {
    set_livre(false);
    set_aguardando_input(false);
    if (!Estacao::ativa())
        Estacao::set_estacao_ativa(this);
    else
        set_status(Status::IS_READY);
    ESTACAO_LOG("disponibilizada para uso");
}

void Estacao::cancelar_receita() {
    reiniciar();
    set_receita_cancelada(true);
    atualizar_campo_gcode(CampoGcode::Atual, "-");
    atualizar_campo_gcode(CampoGcode::Proximo, "-");
    ESTACAO_LOG("receita cancelada");
}

void Estacao::pausar(millis_t duracao) {
    m_pausada = true;
    m_comeco_pausa = millis();
    m_duracao_pausa = duracao;
    set_status(Status::FREE);
    if (esta_ativa())
        procurar_nova_ativa();
    ESTACAO_LOG("pausando por ", duracao, "ms");
}

void Estacao::despausar() {
    m_pausada = false;
    m_comeco_pausa = 0;
    m_duracao_pausa = 0;
}

bool Estacao::tempo_de_pausa_atingido(millis_t tick) const {
    return tick - m_comeco_pausa >= m_duracao_pausa;
}

// isso aqui vai mudar quando tiver a fila
bool Estacao::disponivel_para_uso() const {
    return !bloqueada() && !esta_ativa() && !livre() && !aguardando_input() && !pausada();
}

bool Estacao::esta_ativa() const {
    return Estacao::ativa() == this;
}

void Estacao::atualizar_campo_gcode(CampoGcode qual, std::string_view valor) const {
    static char valor_buffer[64] = {};
    static char nome_buffer[] = "gCode?E?";

    nome_buffer[5] = static_cast<char>(qual) + '0';
    nome_buffer[7] = static_cast<char>(numero()) + '0';

    memcpy(valor_buffer, valor.data(), valor.size());
    valor_buffer[valor.size()] = '\0';
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

void Estacao::set_aguardando_input(bool b) {
    m_aguardando_input = b;
    if (m_aguardando_input)
        set_status(Status::WAITING_START);
}

void Estacao::set_livre(bool b) {
    m_livre = b;
    if (m_livre)
        set_status(Status::FREE);
}

void Estacao::set_bloqueada(bool b) {
    auto antigo = m_bloqueada;
    m_bloqueada = b;
    if (antigo != m_bloqueada) {
        ESTACAO_LOG(m_bloqueada ? "" : "des", "bloqueada");
        reiniciar();
    }
}

void Estacao::reiniciar() {
    m_tick_botao_segurado = 0;
    m_botao_segurado = false;
    m_receita_cancelada = false;
    m_aguardando_input = false;
    m_pausada = false;
    m_comeco_pausa = 0;
    m_duracao_pausa = 0;
    set_livre(true); // hmmm
    if (esta_ativa())
        procurar_nova_ativa();
}
}
