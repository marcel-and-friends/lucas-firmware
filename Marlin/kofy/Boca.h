#pragma once

#include <array>
#include <string>
#include "kofy.h"
#include <string_view>

namespace kofy {

class Boca {
public:

    static constexpr size_t NUM_BOCAS = 5;
    using Lista = std::array<Boca, NUM_BOCAS>;

    static void procurar_nova_boca_ativa();

    static Lista& lista() { return s_lista; }

    static Boca* const boca_ativa() { return s_boca_ativa; }

    static void set_boca_ativa(Boca* boca);

public:
    std::string_view proxima_instrucao() const;

    void progredir_receita();
    
    void pular_proxima_instrucao();

public:
    void set_receita(std::string receita) {
        m_receita = std::move(receita);
        m_progresso_receita = 0;
    }

    void set_botao(int pino) {
        m_pino_botao = pino;
    }

    void set_led(int pino) {
        m_pino_led = pino;
        SET_OUTPUT(pino);
    }

    bool aguardando_botao() const { return m_aguardando_botao; }

    void aguardar_botao() { m_aguardando_botao = true; }

    int botao() const { return m_pino_botao; }

    int led() const { return m_pino_led; }

    const std::string& receita() const { return m_receita; }
    
    ptrdiff_t progresso_receita() const { return m_progresso_receita; }

    void disponibilizar_para_uso();

    size_t numero() const;

private:
    static Lista s_lista;

    static inline Boca* s_boca_ativa = nullptr;

private:
    void finalizar_receita();

    void executar_instrucao(std::string_view instrucao);

    void reiniciar_receita() { m_progresso_receita = 0; }

private:
    // a receita inteira, contém todos os gcodes que vamos executar
    std::string m_receita;

    // a posição dentro da receita que estamos
    ptrdiff_t m_progresso_receita = 0; 
    
    int m_pino_botao = 0;

    int m_pino_led = 0;

    bool m_aguardando_botao = true;
};

}