#pragma once

#include <array>
#include <string>
#include <string_view>
#include <lucas/lucas.h>
#include <lucas/Receita.h>
#include <ArduinoJson.h>

namespace lucas {
class Estacao {
public:
    using Index = size_t;

    static constexpr size_t NUM_MAX_ESTACOES = 5;
    using Lista = std::array<Estacao, NUM_MAX_ESTACOES>;

    static void iniciar(size_t num);

    static void for_each(util::IterCallback<Estacao&> auto&& callback) {
        if (s_num_estacoes == 0)
            return;

        for (size_t i = 0; i < s_num_estacoes; ++i)
            if (std::invoke(FWD(callback), s_lista[i]) == util::Iter::Stop)
                break;
    }

    static void tick(millis_t tick);

    static void procurar_nova_ativa();

    static void set_estacao_ativa(Estacao* estacao);

    static Lista& lista() { return s_lista; }

    static Estacao* const ativa() { return s_estacao_ativa; }

    static float posicao_absoluta(size_t index);

    static size_t num_estacoes() { return s_num_estacoes; }

public:
    void receber_receita(JsonObjectConst json);

    void prosseguir_receita();

    void disponibilizar_para_uso();

    void cancelar_receita();

    bool tempo_de_pausa_atingido(millis_t tick) const;

    bool disponivel_para_uso() const;

    bool esta_ativa() const;

    enum class CampoGcode : char {
        Atual = 0,
        Proximo = 1,
    };

    void atualizar_campo_gcode(CampoGcode, std::string_view str) const;

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

    void gerar_info(JsonObject&) const;

public:
    pin_t botao() const { return m_pino_botao; }
    void set_botao(pin_t pino);

    pin_t led() const { return m_pino_led; }
    void set_led(pin_t pino);

    bool aguardando_input() const { return m_aguardando_input; }
    void set_aguardando_input(bool);

    bool botao_segurado() const { return m_botao_segurado; }
    void set_botao_segurado(bool b) { m_botao_segurado = b; }

    millis_t tick_botao_segurado() const { return m_tick_botao_segurado; }
    void set_tick_botao_segurado(millis_t t) { m_tick_botao_segurado = t; }

    bool livre() const { return m_livre; }
    void set_livre(bool);

    bool receita_cancelada() const { return m_receita_cancelada; }
    void set_receita_cancelada(bool b) { m_receita_cancelada = b; }

    bool pausada() const { return m_pausada; }
    void pausar(millis_t duracao);
    void despausar();

    bool bloqueada() const { return m_bloqueada; }
    void set_bloqueada(bool);

    Status status() const { return m_status; }
    void set_status(Status status) { m_status = status; }

private:
    static Lista s_lista;

    static inline Estacao* s_estacao_ativa = nullptr;

    static inline size_t s_num_estacoes = 0;

private:
    void reiniciar();

private:
    Receita* m_receita = nullptr;

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

    // estacao aguardando input do usuário para prosseguir
    // ocorre no começo de uma receita, após o K0 e no final de uma receita
    bool m_aguardando_input = false;

    // estacao sem receita, livre para ser usada
    bool m_livre = false;

    // se a receita acabou de ser cancelada e o botao ainda nao foi solto
    bool m_receita_cancelada = false;

    // no app voce tem a possibilidade de bloquear um boca
    bool m_bloqueada = false;

    // pausada (pelo L2)
    bool m_pausada = false;
    millis_t m_comeco_pausa = 0;
    millis_t m_duracao_pausa = 0;
};
}
