#pragma once

#include <array>
#include <string>
#include <optional>
#include <string_view>
#include <lucas/lucas.h>
#include <lucas/Receita.h>
#include <ArduinoJson.h>

namespace lucas {
class Estacao {
public:
    using Index = size_t;
    static constexpr size_t INVALIDA = static_cast<size_t>(-1);

    static constexpr size_t NUM_MAX_ESTACOES = 5;
    using Lista = std::array<Estacao, NUM_MAX_ESTACOES>;

    static void inicializar(size_t num);

    static void for_each(util::IterFn<Estacao&> auto&& callback) {
        if (!s_num_estacoes)
            return;

        for (size_t i = 0; i < s_num_estacoes; ++i)
            if (std::invoke(FWD(callback), s_lista[i]) == util::Iter::Break)
                break;
    }

    static void for_each(util::IterFn<Estacao&, size_t> auto&& callback) {
        if (!s_num_estacoes)
            return;

        for (size_t i = 0; i < s_num_estacoes; ++i)
            if (std::invoke(FWD(callback), s_lista[i], i) == util::Iter::Break)
                break;
    }

    static void for_each_if(util::Fn<bool, Estacao&> auto&& condicao, util::IterFn<Estacao&> auto&& callback) {
        if (!s_num_estacoes)
            return;

        for (size_t i = 0; i < s_num_estacoes; ++i) {
            if (std::invoke(FWD(condicao), s_lista[i])) {
                if (std::invoke(FWD(callback), s_lista[i]) == util::Iter::Break)
                    break;
            }
        }
    }

    static void tick();

    static void atualizar_leds();

    static Lista& lista() { return s_lista; }

    static float posicao_absoluta(size_t index);

    static size_t num_estacoes() { return s_num_estacoes; }

    Estacao(const Estacao&) = delete;
    Estacao(Estacao&&) = delete;
    Estacao& operator=(const Estacao&) = delete;
    Estacao& operator=(Estacao&&) = delete;

public:
    // muito importante manter esse enum em sincronia com o app!
    // FREE = 0,
    // WAITING_START,
    // SCALDING,
    // INITIALIZE_COFFEE,
    // MAKING_COFFEE,
    // NOTIFICATION_TIME,
    // IS_READY

    enum class Status {
        Livre = 0,
        ConfirmandoEscaldo,
        Escaldando,
        ConfirmandoAtaques,
        FazendoCafe,
        Finalizando,
        Pronto
    };

    size_t numero() const;
    Index index() const;

    bool aguardando_confirmacao() const { return m_status == Status::ConfirmandoEscaldo || m_status == Status::ConfirmandoAtaques; }
    bool aguardando_input() const { return aguardando_confirmacao() || m_status == Status::Pronto; }

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
    void set_status(Status, std::optional<uint32_t> id_receita = std::nullopt);

private:
    // inicializado out of line porque nesse momento a classe 'Estacao' é incompleta
    static Lista s_lista;

    static inline size_t s_num_estacoes = 0;

private:
    Estacao() = default;

    // status, usado pelo app
    Status m_status = Status::Livre;

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
