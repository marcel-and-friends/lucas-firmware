#include "Fila.h"

namespace lucas {
bool Fila::executando() const {
    return m_index_horizontal < m_receitas.size();
}

void Fila::agendar_receita(Estacao::Index estacao, std::unique_ptr<Receita> receita) {
    // sort the 2d queue like a timeline
    /*
     * 1st vector (vec1) size == 2
     * vec1[0].size() == 1;
     * vec1[1].size() == 2;
     * !VISUAL REPRESENTATION!
     *	------ | ----     ----
     *		   | 	 -----
     */
}

void Fila::prosseguir() {
    auto& fila = m_receitas[m_index_horizontal];
    auto& receita = fila[++m_index_vertical % fila.size()];

    // TODO HANDLE ESCALDO

    if (receita->terminou()) {
        // finalizar - 'Receita::m_finalizacao'
    }

    auto& passo_atual = receita->passo_atual();
    receita->prosseguir();

    // se acabaram todas as receitas dessa index horizontal nós temos que:
    // 1. incrementar m_index_horizontal
    // 	  - se m_index_horizontal == m_receitas.size() então nós terminamos todas as receitas - o que acontece?
    //	      - ao menos limpamos a fila - 'm_receitas.clear()'
}

void Fila::cancelar_receita(Estacao::Index) {
    // come back to this
}
}
