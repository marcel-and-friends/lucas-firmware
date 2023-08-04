#pragma once

#include <lucas/Estacao.h>
#include <lucas/Receita.h>
#include <ArduinoJson.h>
#include <unordered_map>
#include <vector>

namespace lucas {
class Fila {
public:
    static Fila& the() {
        static Fila instance;
        return instance;
    }

    void tick();

    void agendar_receita(JsonObjectConst receita_json);

    void agendar_receita_para_estacao(Receita&, size_t);

    void mapear_receita_da_estacao(size_t);

    void cancelar_receita_da_estacao(size_t);

    void remover_receitas_finalizadas();

    void tentar_aquecer_mangueira_apos_inatividade() const;

    void gerar_informacoes_da_fila(JsonArrayConst estacoes) const;

    size_t estacao_ativa() const { return m_estacao_executando; }

    bool executando() const { return m_estacao_executando != Estacao::INVALIDA; }

public:
    void for_each_receita(util::IterFn<Receita const&, size_t> auto&& callback, Receita const* excecao = nullptr) const {
        if (m_num_receitas == 0) [[likely]]
            return;

        for (size_t i = 0; i < m_fila.size(); ++i) {
            auto& info = m_fila[i];
            if (not info.ativa or (excecao and excecao == &info.receita))
                continue;

            if (std::invoke(FWD(callback), info.receita, i) == util::Iter::Break)
                return;
        }
    };

    void for_each_receita_mapeada(util::IterFn<Receita&> auto&& callback, Receita const* excecao = nullptr) {
        if (m_num_receitas == 0) [[likely]]
            return;

        for (auto& info : m_fila) {
            if (not info.ativa or (excecao and excecao == &info.receita) or not info.receita.passos_pendentes_estao_mapeados())
                continue;

            if (std::invoke(FWD(callback), info.receita) == util::Iter::Break)
                return;
        }
    };

    void for_each_receita_mapeada(util::IterFn<Receita const&> auto&& callback, Receita const* excecao = nullptr) const {
        if (m_num_receitas == 0) [[likely]]
            return;

        for (auto& info : m_fila) {
            if (not info.ativa or (excecao and excecao == &info.receita) or not info.receita.passos_pendentes_estao_mapeados())
                continue;

            if (std::invoke(FWD(callback), info.receita) == util::Iter::Break)
                return;
        }
    };

    void for_each_receita_mapeada(util::IterFn<Receita&, size_t> auto&& callback, Receita const* excecao = nullptr) {
        if (m_num_receitas == 0) [[likely]]
            return;

        for (size_t i = 0; i < m_fila.size(); ++i) {
            auto& info = m_fila[i];
            if (not info.ativa or (excecao and excecao == &info.receita) or not info.receita.passos_pendentes_estao_mapeados())
                continue;

            if (std::invoke(FWD(callback), info.receita, i) == util::Iter::Break)
                return;
        }
    };

    void for_each_receita_mapeada(util::IterFn<Receita const&, size_t> auto&& callback, Receita const* excecao = nullptr) const {
        if (m_num_receitas == 0) [[likely]]
            return;

        for (size_t i = 0; i < m_fila.size(); ++i) {
            auto& info = m_fila[i];
            if (not info.ativa or (excecao and excecao == &info.receita) or not info.receita.passos_pendentes_estao_mapeados())
                continue;

            if (std::invoke(FWD(callback), info.receita, i) == util::Iter::Break)
                return;
        }
    };

private:
    void executar_passo_atual(Receita&, Estacao&);

    void mapear_receita(Receita&, Estacao&);

    void compensar_passo_atrasado(Receita&, Estacao&);

    void remapear_receitas_apos_mudanca_na_fila();

    bool possui_colisoes_com_outras_receitas(Receita const&) const;

    void adicionar_receita(size_t);

    void remover_receita(size_t);

    void receita_cancelada(size_t index);

    size_t numero_de_receitas_em_execucao() const;

private:
    size_t m_estacao_executando = Estacao::INVALIDA;

    struct ReceitaInfo {
        Receita receita;
        bool ativa = false;
    };

    // o mapeamento de index -> receita é o mesmo de index -> estação
    // ou seja, a receita na posição 0 da fila pertence à estação 0
    std::array<ReceitaInfo, Estacao::NUM_MAX_ESTACOES> m_fila = {};
    size_t m_num_receitas = 0;
};
}
