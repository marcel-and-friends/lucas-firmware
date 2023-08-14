#pragma once

#include <src/MarlinCore.h>
#include <lucas/util/util.h>

namespace lucas {
class Estacao;
class Bico {
public:
    static Bico& the() {
        static Bico instance;
        return instance;
    }

    void tick();

    enum class CorrigirFluxo {
        Sim,
        Nao
    };

    using ForcaDigital = uint32_t;

    void despejar_volume(millis_t duracao, float volume_desejado, CorrigirFluxo corrigir);

    float despejar_volume_e_aguardar(millis_t duracao, float volume_desejado, CorrigirFluxo corrigir);

    void despejar_forca_digital(millis_t duracao, ForcaDigital forca_digital);

    void desligar();

    void setup();

    void viajar_para_estacao(Estacao&, float offset = 0.f) const;

    void viajar_para_estacao(size_t, float offset = 0.f) const;

    void aguardar_viagem_terminar() const;

    void viajar_para_esgoto() const;

    void home() const;

    bool ativo() const { return m_ativo; }

    class ControladorFluxo {
    public:
        static constexpr auto FORCA_DIGITAL_INVALIDA = 0xF0F0; // UwU
        static inline auto ML_POR_PULSO = 0.5375f;

        static ControladorFluxo& the() {
            static ControladorFluxo instance;
            return instance;
        }

        void preencher_tabela();

        enum class SalvarNaFlash {
            Sim,
            Nao
        };

        void limpar_tabela(SalvarNaFlash salvar);

        void tentar_copiar_tabela_da_flash();

        ForcaDigital melhor_forca_digital(float fluxo) const;

        static constexpr auto FLUXO_MIN = 5;
        static constexpr auto FLUXO_MAX = 15;
        static constexpr auto RANGE_FLUXO = FLUXO_MAX - FLUXO_MIN;
        static constexpr auto NUMERO_CELULAS = RANGE_FLUXO * 10;
        static_assert(FLUXO_MAX > FLUXO_MIN, "FLUXO_MAX deve ser maior que FLUXO_MIN");

        using Tabela = std::array<std::array<ForcaDigital, 10>, RANGE_FLUXO>;
        static_assert(sizeof(Tabela) == sizeof(ForcaDigital) * NUMERO_CELULAS, "tamanho de tabela inesperado");

    private:
        ControladorFluxo() = default;

        void salvar_tabela_na_flash();
        void copiar_tabela_da_flash();

        struct Fluxo {
            float fluxo;
            ForcaDigital forca_digital;
        };

        Fluxo primeiro_fluxo_abaixo(int fluxo_index, int casa_decimal) const;
        Fluxo primeiro_fluxo_acima(int fluxo_index, int casa_decimal) const;

        struct FluxoDecomposto {
            int fluxo_inteiro;
            int casa_decimal;
        };

        FluxoDecomposto decompor_fluxo(float fluxo) const;

        size_t numero_celulas() const {
            size_t num = 0;
            for_each_celula([&num](auto) { ++num; return util::Iter::Continue; });
            return num;
        }

        void for_each_celula(util::IterFn<ForcaDigital> auto&& callback) const {
            for (size_t i = 0; i < m_tabela.size(); ++i) {
                auto& linha = m_tabela[i];
                for (size_t j = 0; j < linha.size(); ++j) {
                    auto forca_digital = linha[j];
                    if (forca_digital != FORCA_DIGITAL_INVALIDA)
                        if (std::invoke(FWD(callback), forca_digital) == util::Iter::Break)
                            return;
                }
            }
        }

        void for_each_celula(util::IterFn<ForcaDigital, size_t, size_t> auto&& callback) const {
            for (size_t i = 0; i < m_tabela.size(); ++i) {
                auto& linha = m_tabela[i];
                for (size_t j = 0; j < linha.size(); ++j) {
                    auto forca_digital = linha[j];
                    if (forca_digital != FORCA_DIGITAL_INVALIDA)
                        if (std::invoke(FWD(callback), forca_digital, i, j) == util::Iter::Break)
                            return;
                }
            }
        }

        // essa tabela é como uma matriz de todos os valores digitais que produzem os fluxos disponiveis entre FLUXO_MIN e FLUXO_MAX, com 1 digito de precisão
        // por exemplo, assumindo que FLUXO_MIN == 5:
        // 		- a forca digital para o fluxo `5.5` se encontra em `m_tabela[0][4]`
        // 		- a forca digital para o fluxo `10.3` se encontra em `m_tabela[5][2]`
        // os fluxos encontrados são arrendondados apropriadamente e encaixados na célula mais próxima
        // @ref `decompor_fluxo` - `valor_na_tabela`
        Tabela m_tabela = { {} };
    };

private:
    void aplicar_forca(ForcaDigital);

    void iniciar_despejo(millis_t duracao);

    void preencher_mangueira(float possivel_fluxo_desejado = 0.f);

    static volatile inline uint32_t s_contador_de_pulsos = 0;
    uint32_t m_pulsos_no_inicio_do_despejo = 0;
    uint32_t m_pulsos_no_final_do_despejo = 0;

    millis_t m_duracao = 0;

    float m_volume_total_desejado = 0.f;

    CorrigirFluxo m_corrigir_fluxo_durante_despejo = CorrigirFluxo::Nao;

    millis_t m_ultimo_ajuste_fluxo = 0;

    millis_t m_tempo_decorrido = 0;

    millis_t m_tick_comeco = 0;
    millis_t m_tick_final = 0;

    ForcaDigital m_forca = 0;

    bool m_ativo = false;
};
}
