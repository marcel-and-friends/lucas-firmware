#pragma once

#include <lucas/Estacao.h>
#include <lucas/Receita.h>
#include <vector>

namespace lucas {
class Fila {
public:
    static Fila& the() {
        static Fila instance;
        return instance;
    }

    bool executando() const;

    void agendar_receita(Estacao::Index, std::unique_ptr<Receita> receita);

    void cancelar_receita(Estacao::Index);

    void prosseguir();

private:
    size_t m_index_horizontal = 0;

    size_t m_index_vertical = 0;

    std::vector<std::vector<std::unique_ptr<Receita>>> m_receitas;
};
}
