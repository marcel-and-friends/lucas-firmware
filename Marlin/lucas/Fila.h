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
        static Fila instance(Estacao::NUM_MAX_ESTACOES);
        return instance;
    }

    void for_each_receita_mapeada(util::IterCallback<Receita&> auto&& callback, Receita* excecao = nullptr) {
        for (auto& [_, receita] : m_fila) {
            if ((excecao && receita.get() == excecao) || !receita->passos_pendentes_estao_mapeados())
                continue;

            if (std::invoke(FWD(callback), *receita) == util::Iter::Stop)
                return;
        }
    };

    void for_each_receita_mapeada(util::IterCallback<Receita&, Estacao::Index> auto&& callback, Receita* excecao = nullptr) {
        for (auto& [estacao_idx, receita] : m_fila) {
            if ((excecao && receita.get() == excecao) || !receita->passos_pendentes_estao_mapeados())
                continue;

            if (std::invoke(FWD(callback), *receita, estacao_idx) == util::Iter::Stop)
                return;
        }
    };

    void tick(millis_t tick);

    void agendar_receita(Estacao::Index, std::unique_ptr<Receita> receita);

    void mapear_receita(Estacao::Index);

    void cancelar_receita(Estacao::Index);

    Estacao::Index estacao_ativa() { return m_estacao_ativa; }

    Estacao::Index executando() { return m_estacao_ativa != Estacao::INVALIDA; }

    void gerar_info(JsonObject& json);

private:
    explicit Fila(size_t num_estacoes) {
        m_fila.reserve(num_estacoes);
    }

    void mapear_receita(Estacao::Index, Receita&);

    bool possui_colisoes_com_outras_receitas(Receita& receita);

    size_t m_index_horizontal = 0;

    size_t m_index_vertical = 0;

    Estacao::Index m_estacao_ativa = Estacao::INVALIDA;

    std::unordered_map<Estacao::Index, std::unique_ptr<Receita>> m_fila;
};
}
