#pragma once

#include <array>
#include <string>
#include <string_view>
#include <lucas/lucas.h>
#include <lucas/Receita.h>
#include <lucas/info/evento.h>
#include <ArduinoJson.h>

namespace lucas {
class Estacao {
public:
    using Index = size_t;
    static constexpr size_t INVALIDA = static_cast<size_t>(-1);

    static constexpr size_t NUM_MAX_ESTACOES = 5;
    using Lista = std::array<Estacao, NUM_MAX_ESTACOES>;

    static void iniciar(size_t num);

    static void for_each(util::IterCallback<Estacao&> auto&& callback) {
        if (s_num_estacoes == 0)
            return;

        for (size_t i = 0; i < s_num_estacoes; ++i)
            if (std::invoke(FWD(callback), s_lista[i]) == util::Iter::Break)
                break;
    }

    static void for_each_if(auto&& condicao, util::IterCallback<Estacao&> auto&& callback) {
        if (s_num_estacoes == 0)
            return;

        for (size_t i = 0; i < s_num_estacoes; ++i) {
            if (std::invoke(FWD(condicao), s_lista[i])) {
                if (std::invoke(FWD(callback), s_lista[i]) == util::Iter::Break)
                    break;
            }
        }
    }

    static void tick();

    static Lista& lista() { return s_lista; }

    static float posicao_absoluta(size_t index);

    static size_t num_estacoes() { return s_num_estacoes; }

public:
    enum class Status {
        FREE = 0,
        WAITING_START,
        SCALDING,
        INITIALIZE_COFFEE,
        MAKING_COFFEE,
        NOTIFICATION_TIME,
        IS_READY
    };

    size_t numero() const;
    Index index() const;

    bool aguardando_input() const { return m_status == Status::WAITING_START || m_status == Status::INITIALIZE_COFFEE || m_status == Status::IS_READY; }

public:
    pin_t botao() const { return m_pino_botao; }
    void set_botao(pin_t pino);

    pin_t led() const { return m_pino_led; }
    void set_led(pin_t pino);

    bool botao_segurado() const { return m_botao_segurado; }
    void set_botao_segurado(bool b) { m_botao_segurado = b; }

    millis_t tick_botao_segurado() const { return m_tick_botao_segurado; }
    void set_tick_botao_segurado(millis_t t) { m_tick_botao_segurado = t; }

    bool receita_cancelada() const { return m_receita_cancelada; }
    void set_receita_cancelada(bool b) { m_receita_cancelada = b; }

    bool bloqueada() const { return m_bloqueada; }
    void set_bloqueada(bool);

    Status status() const { return m_status; }
    void set_status(Status);

public:
    class NovoStatus : public info::Evento<NovoStatus, "novoStatus"> {
    public:
        void gerar_json_impl(JsonObject o) const {
            o["estacao"] = estacao;
            o["status"] = static_cast<int>(novo_status);
        }

        size_t estacao;
        Status novo_status;
    };

private:
    static Lista s_lista;

    static inline size_t s_num_estacoes = 0;

private:
    // status, usado pelo app
    Status m_status = Status::FREE;

    // o pino físico do nosso botão
    pin_t m_pino_botao = 0;

    // o pino físico da nossa led
    pin_t m_pino_led = 0;

    // o ultimo tick em que o botao foi apertado
    // usado para cancelar uma receita
    millis_t m_tick_botao_segurado = 0;

    // usado como um debounce para ativar uma estacao somente quando o botão é solto
    bool m_botao_segurado = false;

    // se a receita acabou de ser cancelada e o botao ainda nao foi solto
    bool m_receita_cancelada = false;

    // no app voce tem a possibilidade de bloquear um boca
    bool m_bloqueada = false;
};
}
