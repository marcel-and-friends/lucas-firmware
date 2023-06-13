#pragma once

#include <lucas/Estacao.h>
#include <lucas/Receita.h>
#include <unordered_map>
#include <vector>

namespace lucas {
class Fila {
public:
    static Fila& the() {
        static Fila instance;
        return instance;
    }

    void tick(millis_t tick);

    bool executando() const;

    void agendar_receita(Estacao::Index, std::unique_ptr<Receita> receita);

    void mapear_receita(Estacao::Index);

    void cancelar_receita(Estacao::Index);

    void for_each_receita(auto&& callback) {
        for (auto& [estacao_idx, receita] : m_receitas)
            if (std::invoke(callback, estacao_idx, *receita) == util::Iter::Stop)
                break;
    }

private:
    void mapear_escaldo(Estacao::Index);

    size_t m_index_horizontal = 0;

    size_t m_index_vertical = 0;

    Receita* m_receita_ativa = nullptr;

    std::unordered_map<Estacao::Index, std::unique_ptr<Receita>> m_receitas;
};
}
