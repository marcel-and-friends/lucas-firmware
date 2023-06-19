#pragma once

#include <src/MarlinCore.h>
#include <lucas/Estacao.h>

namespace lucas {
class Bico {
public:
    static Bico& the() {
        static Bico instance;
        return instance;
    }

    void tick(millis_t tick);

    void ativar(millis_t tick, millis_t tempo, float gramas);

    void setup();

    void viajar_para_estacao(Estacao&) const;

    void viajar_para_estacao(Estacao::Index) const;

    bool esta_na_estacao(Estacao::Index) const;

    void viajar_para_esgoto() const;

    void descartar_agua_ruim() const;

    void nivelar() const;

    bool ativo() const {
        return m_ativo;
    }

private:
    void desligar(millis_t tick_desligou);

    // duracao do despejo
    millis_t m_duracao = 0;
    // instante onde o despejo comeca/acaba
    millis_t m_tick_comeco = 0;
    millis_t m_tick_final = 0;
    // valor digital enviado para o driver do motor
    uint32_t m_forca = 0;
    // se estamos despejando ou nao
    bool m_ativo = false;
};

}
