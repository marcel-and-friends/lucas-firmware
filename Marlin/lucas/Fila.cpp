#include "Fila.h"
#include <lucas/Bico.h>
#include <numeric>
#include <ranges>

namespace lucas {
#define FILA_LOG(...) LOG("", "fila - ", __VA_ARGS__);

void Fila::agendar_receita(Estacao::Index estacao_idx, std::unique_ptr<Receita> receita_ptr) {
    if (m_fila.contains(estacao_idx)) {
        FILA_LOG("agendando receita pra estacao que ja tem receita agendada");
        return;
    }

    auto& receita = *receita_ptr;
    m_fila.emplace(estacao_idx, std::move(receita_ptr));
    mapear_receita(estacao_idx, receita);
    return;
    if (m_fila.size() == 1) {
        auto& estacao = Estacao::lista().at(estacao_idx);
        Bico::the().viajar_para_estacao(estacao);
        if (receita.possui_escaldo()) {
            FILA_LOG("receita possui escaldo");
            estacao.set_status(Estacao::Status::SCALDING);

            m_receita_ativa = &receita;
            receita.executar_escaldo();
            m_receita_ativa = nullptr;

            estacao.set_status(Estacao::Status::INITIALIZE_COFFEE);
            estacao.set_aguardando_input(true);
            FILA_LOG("escaldo finalizado");
        }
    } else {
        LOG("mapeando receita pra varias coisa");
    }
}

void Fila::tick(millis_t tick) {
    if (m_receita_ativa || m_fila.empty())
        return;

    for (auto& [estacao_idx, receita] : m_fila) {
        const auto comeco = receita->ataque_atual().comeco_abs;
        if (comeco == tick) {
            FILA_LOG("ataque bateu! [tick = ", tick, "]");

            m_receita_ativa = receita.get();
            receita->executar_ataque();
            if (receita->terminou()) {
                m_fila.erase(estacao_idx);
                FILA_LOG("receita acabou! removida da fila");
            }
            m_receita_ativa = nullptr;
            break;
        } else if (comeco - tick == MARGEM_DE_VIAGEM) {
            Bico::the().viajar_para_estacao(estacao_idx + 1);
            break;
        }
    };
}

void Fila::mapear_receita(Estacao::Index estacao_idx) {
    auto it = m_fila.find(estacao_idx);
    if (it == m_fila.end()) {
        FILA_LOG("receita nao encontrada para estacao #", estacao_idx);
        return;
    }
    mapear_receita(estacao_idx, *it->second);
}

void Fila::mapear_receita(Estacao::Index estacao_idx, Receita& receita) {
    // se só temos essa receita na fila ela é executada imediatamente
    if (m_fila.size() == 1) {
        FILA_LOG("mapeando sozinho os ataques da estacao #", estacao_idx);
        // garantimos que o bico já estará na estação antes de comecarmos
        Bico::the().viajar_para_estacao(estacao_idx + 1);
        receita.mapear_ataques(millis() + 2000); // 5ms de gordurinha pa nao ter b.o
    } else {
        bool sucesso = false;
        for (auto& [estacao_idx_mapeada, receita_mapeada] : m_fila) {
            if (estacao_idx_mapeada == estacao_idx) {
                LOG("nao vou usar eu mesmo ne porra");
                continue;
            }
            // uma receita na fila que ainda não foi escaldada não pode ser usada
            // if (receita_mapeada->possui_escaldo() && !receita_mapeada->escaldou())
            //     continue;

            receita_mapeada->for_each_ataque_pendente([&](const Receita::Passo& ataque, size_t i) {
                const auto fim = ataque.fim();
                for (auto offset_intervalo = MARGEM_DE_VIAGEM; offset_intervalo <= ataque.intervalo - MARGEM_DE_VIAGEM; offset_intervalo += MARGEM_DE_VIAGEM) {
                    receita.mapear_ataques(fim + offset_intervalo);
                    if (!possui_colisoes_com_outras_receitas(receita)) {
                        sucesso = true;
                        return util::Iter::Stop;
                    }
                }
                return util::Iter::Continue;
            });

            if (sucesso)
                break;
        }

        if (!sucesso) {
            FILA_LOG("nao foi possivel mapear receita para estacao #", estacao_idx);
            m_fila.erase(estacao_idx);
        }
    }
}

void Fila::cancelar_receita(Estacao::Index) {
    // come back to this
}

bool Fila::possui_colisoes_com_outras_receitas(Receita& receita_experimental) {
    bool colide = false;
    int i = 0;
    for (auto& ataque_experimental : receita_experimental.ataques()) {
        for (auto& [idx, receita] : m_fila) {
            if (&receita_experimental == receita.get())
                continue;

            receita->for_each_ataque_pendente([&](Receita::Passo& ataque_mapeado, size_t i) {
                if (ataque_mapeado.comeco_abs >= ataque_experimental.fim())
                    return util::Iter::Stop;

                if ((ataque_experimental.comeco_abs >= ataque_mapeado.comeco_abs && ataque_experimental.comeco_abs <= ataque_mapeado.fim()) ||
                    (ataque_experimental.fim() >= ataque_mapeado.comeco_abs && ataque_experimental.fim() <= ataque_mapeado.fim())) {
                    colide = true;
                    return util::Iter::Stop;
                }

                return util::Iter::Continue;
            });

            if (colide)
                return true;
        }

        i++;
    }

    return colide;
}

}
