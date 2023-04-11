#pragma once

#include <array>
#include <string>
#include <string_view>
#include <kofy/kofy.h>

namespace kofy {

class Boca {
public:

    static constexpr size_t NUM_BOCAS = 5;
    using Lista = std::array<Boca, NUM_BOCAS>;

    static void procurar_nova_boca_ativa();

    static Lista& lista() { return s_lista; }

    static Boca* const ativa() { return s_boca_ativa; }

    static void set_boca_ativa(Boca* boca);

public:
    std::string_view proxima_instrucao() const;

    void prosseguir_receita();

    void disponibilizar_para_uso();

    size_t numero() const;

	void cancelar_receita();

public:
    void set_receita(std::string receita) {
        m_receita = std::move(receita);
        m_progresso_receita = 0;
    }

    void set_botao(int pino) { m_pino_botao = pino; }

    void set_led(int pino) {
        m_pino_led = pino;
        SET_OUTPUT(pino);
    }

	void set_botao_apertado(bool b) { m_botao_apertado = b; }

    bool aguardando_botao() const { return m_aguardando_botao; }

	bool botao_apertado() const { return m_botao_apertado; }

    void aguardar_botao() { m_aguardando_botao = true; }

    int botao() const { return m_pino_botao; }

    int led() const { return m_pino_led; }

    const std::string& receita() const { return m_receita; }

    ptrdiff_t progresso_receita() const { return m_progresso_receita; }

	void reiniciar_receita() { m_progresso_receita = 0; }

	millis_t tick_apertado_para_cancelar() const { return m_tick_apertado_para_cancelar; }

	void set_tick_apertado_para_cancelar(millis_t t) { m_tick_apertado_para_cancelar = t; }

private:
    static Lista s_lista;

    static inline Boca* s_boca_ativa = nullptr;

private:
    void finalizar_receita();

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

	// estamos aguardando o botão ser apertado para prosseguirmos?
	// OBS: todas as máquinas começam aguardando o botão
    bool m_aguardando_botao = true;

	// estamos segurando o botão?
	// usado como um debounce para ativar uma boca somente quando o botão é solto
	bool m_botao_apertado = false;

	// quanto tempo o botão está sendo segurado
	// usado para cancelar uma receita
	millis_t m_tick_apertado_para_cancelar = 0;
};

}
