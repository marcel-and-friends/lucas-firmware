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

    void tick();

    void despejar_volume(millis_t duracao, float volume_desejado);

    void desepejar_com_valor_digital(millis_t duracao, uint32_t valor_digital);

    void desligar();

    void setup();

    void viajar_para_estacao(Estacao&, int offset = 0) const;

    void viajar_para_estacao(Estacao::Index, int offset = 0) const;

    bool esta_na_estacao(Estacao::Index) const;

    void viajar_para_esgoto() const;

    void descartar_agua_ruim() const;

    void nivelar() const;

    void preencher_tabela_de_controle_de_fluxo() const;

    bool ativo() const {
        return m_ativo;
    }

private:
    struct ControladorFluxo {
        static constexpr auto FLUXO_MIN = 5;
        static constexpr auto FLUXO_MAX = 15;
        static constexpr auto RANGE_FLUXO = FLUXO_MAX - FLUXO_MIN;

        static constexpr auto VALOR_DIGITAL_INVALIDO = 0xF0F0;
        static constexpr auto ML_POR_PULSO = 0.55f;

        static ControladorFluxo& the() {
            static ControladorFluxo instance;
            return instance;
        }
        void preencher_tabela();

        uint32_t melhor_valor_digital(float fluxo);

    private:
        ControladorFluxo() = default;

        void limpar_tabela();

        std::tuple<float, uint32_t> primeiro_fluxo_abaixo(int fluxo_index, int casa_decimal);

        std::tuple<float, uint32_t> primeiro_fluxo_acima(int fluxo_index, int casa_decimal);

        int casa_decimal_apropriada(float fluxo);

        std::tuple<int, uint32_t> decompor_fluxo(float fluxo);

        uint32_t& valor_na_tabela(float fluxo);

        std::array<std::array<uint32_t, 10>, RANGE_FLUXO> m_tabela = { {} };
    };

    void aplicar_forca(uint32_t);

    void iniciar_despejo(millis_t duracao);

    static inline uint64_t s_contador_de_pulsos = 0;

    millis_t m_duracao = 0;

    float m_volume_total_desejado = 0.f;

    millis_t m_tempo_decorrido = 0;

    uint64_t m_pulsos_no_inicio_do_despejo = 0;

    uint64_t m_pulsos_no_final_do_despejo = 0;

    // instante onde o despejo comeca/acaba
    millis_t m_tick_comeco = 0;
    millis_t m_tick_final = 0;

    // valor digital enviado para o driver do motor
    uint32_t m_forca = 0;

    // se estamos despejando ou nao
    bool m_ativo = false;
};
}
