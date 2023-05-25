#pragma once

#include <array>
#include <string>
#include <string_view>
#include <lucas/lucas.h>

namespace lucas {
class Estacao {
public:
    static constexpr size_t NUM_ESTACOES = 2;
    using Lista = std::array<Estacao, NUM_ESTACOES>;

    static void procurar_nova_ativa();

    static void set_estacao_ativa(Estacao* estacao);

    static Lista& lista() { return s_lista; }

    static Estacao* const ativa() { return s_estacao_ativa; }

public:
    void enviar_receita(std::string receita, size_t id);

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

    void atualizar_status(const char* str) const;

    float posicao_absoluta() const;

    size_t numero() const;

    size_t index() const;

public:
    int botao() const { return m_pino_botao; }
    void set_botao(int pino) {
        m_pino_botao = pino;
        SET_INPUT_PULLUP(pino);
    }

    bool aguardando_input() const { return m_aguardando_input; }
    void set_aguardando_input(bool);

    bool botao_apertado() const { return m_botao_apertado; }
    void set_botao_apertado(bool b) { m_botao_apertado = b; }

    int led() const { return m_pino_led; }
    void set_led(int pino) {
        m_pino_led = pino;
        SET_OUTPUT(pino);
    }

    const std::string& receita() const { return m_receita_gcode; }
    void set_receita(std::string receita, size_t id);

    ptrdiff_t progresso_receita() const { return m_receita_progresso; }
    void reiniciar_receita() { m_receita_progresso = 0; }

    millis_t tick_botao_segurado() const { return m_tick_botao_segurado; }
    void set_tick_botao_segurado(millis_t t) { m_tick_botao_segurado = t; }

    bool livre() const { return m_livre; }
    void set_livre(bool);

    bool receita_cancelada() const { return m_receita_cancelada; }
    void set_receita_cancelada(bool b) { m_receita_cancelada = b; }

    void pausar(millis_t duracao);
    void despausar();
    bool pausada() const { return m_pausada; }

    void set_bloqueada(bool);
    bool bloqueada() const { return m_bloqueada; }

private:
    static Lista s_lista;

    static inline Estacao* s_estacao_ativa = nullptr;

private:
    std::string_view proxima_instrucao() const;

    void reiniciar();

private:
    struct InfoAtaque {
        millis_t duracao = 0;
        millis_t intervalo = 0;
        bool ultimo = false;
    };
    struct InfoReceita {
        std::vector<InfoAtaque> ataques;
    };

    InfoReceita m_receita_info;

    // a receita inteira, contém todos os gcodes que vamos executar
    std::string m_receita_gcode;

    // a posição dentro da receita que estamos
    ptrdiff_t m_receita_progresso = 0;

    // id dela para o app
    size_t m_receita_id;

    // o pino físico do nosso botão
    int m_pino_botao = 0;

    // o pino físico da nossa led
    int m_pino_led = 0;

    // o ultimo tick em que o botao foi apertado
    // usado para cancelar uma receita
    millis_t m_tick_botao_segurado = 0;

    // estacao aguardando input do usuário para prosseguir
    // ocorre no começo de uma receita, após o K0 e no final de uma receita
    bool m_aguardando_input = false;

    // estacao sem receita, livre para ser usada
    bool m_livre = false;

    // usado como um debounce para ativar uma estacao somente quando o botão é solto
    bool m_botao_apertado = false;

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
