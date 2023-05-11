#pragma once

#include <array>
#include <string>
#include <string_view>
#include <lucas/lucas.h>

namespace lucas {
class Estacao {
public:
    static constexpr size_t NUM_ESTACOES = 5;
    using Lista = std::array<Estacao, NUM_ESTACOES>;

    static void procurar_nova_ativa();

    static void set_estacao_ativa(Estacao* estacao);

    static void ativa_prestes_a_comecar();

    static Lista& lista() { return s_lista; }

    static Estacao* const ativa() { return s_estacao_ativa; }

public:
    void prosseguir_receita();

    void preparar_para_finalizar_receita();

    void disponibilizar_para_uso();

    void aguardar_input();

    void cancelar_receita();

    void receita_finalizada();

    void pausar(millis_t duracao);

    bool tempo_de_pausa_atingido(millis_t tick) const;

    bool disponivel_para_uso() const;

    bool esta_ativa() const;

    enum class CampoGcode : char {
        Atual = 0,
        Proximo = 1,
    };

    void atualizar_campo_gcode(CampoGcode, std::string_view str) const;

    void atualizar_status(std::string_view str) const;

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
    void set_aguardando_input(bool b);

    bool botao_apertado() const { return m_botao_apertado; }
    void set_botao_apertado(bool b) { m_botao_apertado = b; }

    int led() const { return m_pino_led; }
    void set_led(int pino) {
        m_pino_led = pino;
        SET_OUTPUT(pino);
    }

    const std::string& receita() const { return m_receita; }
    void set_receita(std::string receita) {
        m_receita = std::move(receita);
        m_progresso_receita = 0;
    }

    ptrdiff_t progresso_receita() const { return m_progresso_receita; }
    void reiniciar_receita() { m_progresso_receita = 0; }

    millis_t tick_apertado_para_cancelar() const { return m_tick_apertado_para_cancelar; }
    void set_tick_apertado_para_cancelar(millis_t t) { m_tick_apertado_para_cancelar = t; }

    bool livre() const { return m_livre; }
    void set_livre(bool b) { m_livre = b; }

    bool cancelada() const { return m_cancelada; }
    void set_cancelada(bool b) { m_cancelada = b; }

    bool pausada() const { return m_pausada; }

private:
    static Lista s_lista;

    static inline Estacao* s_estacao_ativa = nullptr;

    static void executar_rotina_troca_de_estacao_ativa();

private:
    std::string_view proxima_instrucao() const;

    void executar_instrucao(std::string_view instrucao);

    void reiniciar_progresso() { m_progresso_receita = 0; }

private:
    // a receita inteira, contém todos os gcodes que vamos executar
    std::string m_receita;

    // a posição dentro da receita que estamos
    ptrdiff_t m_progresso_receita = 0;

    // o pino físico do nosso botão
    int m_pino_botao = 0;

    // o pino físico da nossa led
    int m_pino_led = 0;

    // o ultimo tick em que o botao foi apertado
    // usado para cancelar uma receita
    millis_t m_tick_apertado_para_cancelar = 0;

    millis_t m_comeco_pausa = 0;

    millis_t m_duracao_pausa = 0;

    // estacao aguardando input do usuário para prosseguir
    // ocorre no começo de uma receita, após o K0 e no final de uma receita
    bool m_aguardando_input = false;

    // estacao sem receita, livre para ser usada
    bool m_livre = false;

    // as estacoes podem ser desabilitadas pelo app
    bool m_habilitada = true;

    // usado como um debounce para ativar uma estacao somente quando o botão é solto
    bool m_botao_apertado = false;

    bool m_cancelada = false;

    bool m_pausada = false;
};
}
