#pragma once

#include <src/MarlinCore.h>

namespace lucas {
class Estacao;
class Bico {
public:
    static Bico& the() {
        static Bico the;
        return the;
    }

    void tick(millis_t tick);

    void ativar(millis_t tick, millis_t tempo, float gramas);

    void setup();

    void viajar_para_estacao(Estacao&) const;

    void viajar_para_estacao(size_t numero) const;

    void descartar_agua_ruim() const;

    void nivelar() const;

    bool ativo() const {
        return m_ativo;
    }

private:
    void reset();

    millis_t m_tempo = 0;

    millis_t m_tick_desligou = 0;

    millis_t m_tick = 0;

    uint32_t m_poder = 0;

    bool m_ativo = false;
};

}
