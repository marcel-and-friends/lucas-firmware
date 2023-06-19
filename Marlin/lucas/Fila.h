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

    void agendar_receita(Estacao::Index, std::unique_ptr<Receita> receita);

    void mapear_receita(Estacao::Index);

    void cancelar_receita(Estacao::Index);

    bool receita_esta_mapeada(const Estacao& estacao) const {
        return !m_fila[estacao.index()].inativa &&
               m_fila[estacao.index()].receita &&
               m_fila[estacao.index()].receita->passos_pendentes_estao_mapeados();
    }

    Estacao::Index estacao_ativa() const { return m_estacao_ativa; }

    bool executando() const { return m_estacao_ativa != Estacao::INVALIDA; }

    void gerar_info(millis_t tick, JsonObject obj) const;

public:
    void for_each_receita_mapeada(util::IterCallback<Receita&> auto&& callback, Receita* excecao = nullptr) {
        if (!m_num_receitas)
            return;

        for (auto& info : m_fila) {
            if (!info.valida() || (excecao && info.receita.get() == excecao) || !(info.receita->passos_pendentes_estao_mapeados()))
                continue;

            if (std::invoke(FWD(callback), *info.receita) == util::Iter::Break)
                return;
        }
    };

    void for_each_receita_mapeada(util::IterCallback<Receita&, Estacao::Index> auto&& callback, Receita* excecao = nullptr) {
        if (!m_num_receitas)
            return;

        for (size_t i = 0; i < m_fila.size(); ++i) {
            auto& info = m_fila[i];
            if (!info.valida() || (excecao && info.receita.get() == excecao) || !(info.receita->passos_pendentes_estao_mapeados()))
                continue;

            if (std::invoke(FWD(callback), *info.receita, i) == util::Iter::Break)
                return;
        }
    };

private:
    bool possui_colisoes_com_outras_receitas(Receita&);

    void remover_receita(Estacao::Index);

    void adicionar_receita(std::unique_ptr<Receita>, Estacao::Index);

    Estacao::Index m_estacao_ativa = Estacao::INVALIDA;

    // para quando não há receita ativa mas não queremos executar a fila
    bool m_ocupado = false;

    struct ReceitaInfo {
        std::unique_ptr<Receita> receita = nullptr;
        bool inativa = true;

        bool valida() const {
            return !inativa && receita;
        }
    };

    // isso aqui modela meio que um hashmap mas nao quero alocar
    std::array<ReceitaInfo, Estacao::NUM_MAX_ESTACOES> m_fila = {};
    size_t m_num_receitas = 0;
};
}
