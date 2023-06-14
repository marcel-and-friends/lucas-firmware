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

    void agendar_receita(Estacao::Index, std::unique_ptr<Receita> receita);

    void mapear_receita(Estacao::Index);

    void cancelar_receita(Estacao::Index);


    static constexpr millis_t MARGEM_DE_VIAGEM = 2000;

private:
    void mapear_receita(Estacao::Index, Receita&);

    bool possui_colisoes_com_outras_receitas(Receita& receita);

    size_t m_index_horizontal = 0;

    size_t m_index_vertical = 0;

    Receita* m_receita_ativa = nullptr;

    std::unordered_map<Estacao::Index, std::unique_ptr<Receita>> m_fila;
};
}
