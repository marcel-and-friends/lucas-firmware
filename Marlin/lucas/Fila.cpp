#include "Fila.h"
#include <lucas/Bico.h>
#include <src/module/planner.h>
#include <numeric>
#include <ranges>

namespace lucas {
#define FILA_LOG(...) LOG("", "FILA: ", __VA_ARGS__);

// 1s de "margem de erro" pra dar tempo da placa calcular tudo
constexpr auto MARGEM_CALCULO = 1000;

void Fila::agendar_receita(Estacao::Index estacao_idx, std::unique_ptr<Receita> receita_ptr) {
    if (m_fila.contains(estacao_idx)) {
        FILA_LOG("ERRO - estacao ja possui uma receita na fila - [estacao = ", estacao_idx, "]");
        return;
    }

    m_fila.emplace(estacao_idx, std::move(receita_ptr));
    mapear_receita(estacao_idx, *m_fila[estacao_idx]);
}

void Fila::tick(millis_t tick) {
    if (m_estacao_ativa != Estacao::INVALIDA) {
        auto it = m_fila.find(m_estacao_ativa);
        if (it == m_fila.end()) {
            FILA_LOG("ERRO - estacao ativa nao esta na fila - [estacao = ", m_estacao_ativa, "]");
            m_estacao_ativa = Estacao::INVALIDA;
            return;
        }
        auto& receita = *it->second;
        receita.passo_atual().progresso = tick - receita.passo_atual().comeco_abs;
        return;
    }

    for_each_receita_mapeada([this, tick](Receita& receita, Estacao::Index estacao_idx) {
        const auto comeco = receita.passo_atual().comeco_abs;
        // o passo está chegando...
        if (comeco - tick <= util::MARGEM_DE_VIAGEM) {
            if (!Bico::the().esta_na_estacao(estacao_idx) && !planner.busy())
                // colocamos o bico na estacao antes do passo chegar
                Bico::the().viajar_para_estacao(estacao_idx);
        }
        // o passo chegou!
        else if (comeco == tick) {
            FILA_LOG("passo bateu! - [tick = ", tick, " | estacao = ", estacao_idx, "]");

            m_estacao_ativa = estacao_idx;
            if (receita.executar_passo() == Receita::Acabou::Sim) {
                m_fila.erase(m_estacao_ativa);
                FILA_LOG("receita acabou e foi removida da fila - [estacao = ", estacao_idx, "]");
            }
            m_estacao_ativa = Estacao::INVALIDA;
            return util::Iter::Stop;
        }
        // perdemos o passo :(
        // isso é um caso raro que, em teoria, nunca deve acontecer
        // mas se acontecer, vamos tentar recuperar o mais rápido possível
        else if (comeco < tick) {
            // primeiro viajamos para a receita atrasada
            Bico::the().viajar_para_estacao(estacao_idx);
            const auto tick_inicial = millis() + MARGEM_CALCULO;
            const auto delta = tick_inicial - comeco;
            FILA_LOG("ERRO - perdemos um tick, deslocando todas as receitas! - [delta =  ", delta, "ms]");
            // agora mapeamos os passos da receita atrasada para daqui a pouquinho
            receita.mapear_passos_pendentes(tick_inicial);
            // e adicionamos um offset em todos os passos restantes
            // é melhor atrasarmos todas as receitas um pouquinho do que atrasarmos *muito* uma unica receita
            for_each_receita_mapeada(
                [delta](Receita& outra_receita) {
                    outra_receita.for_each_passo_pendente([delta](Receita::Passo& passo) {
                        passo.comeco_abs += delta;
                        return util::Iter::Continue;
                    });
                    return util::Iter::Continue;
                },
                &receita);
        }
        return util::Iter::Continue;
    });
}

void Fila::mapear_receita(Estacao::Index estacao_idx) {
    auto it = m_fila.find(estacao_idx);
    if (it == m_fila.end()) {
        FILA_LOG("ERRO - estacao nao possui receita na fila - [estacao = ", estacao_idx, "]");
        return;
    }

    if (it->second->passos_pendentes_estao_mapeados()) {
        FILA_LOG("receita da estacao #", estacao_idx, "ja esta mapeada");
        return;
    }

    mapear_receita(estacao_idx, *it->second);
}

void Fila::mapear_receita(Estacao::Index estacao_idx, Receita& receita) {
    // finge que isso é um vector
    std::array<millis_t, Estacao::NUM_MAX_ESTACOES> candidatos = {};
    size_t num_candidatos = 0;

    // tentamos encontrar um tick inicial que não causa colisões com nenhuma das outras receita
    // esse processo é feito de forma bruta, homem das cavernas, mas como o número de receitas/passos é pequeno, não chega a ser um problema
    for_each_receita_mapeada(
        [&](Receita& receita_mapeada) {
            receita_mapeada.for_each_passo_pendente([&](const Receita::Passo& passo) {
                // temos que sempre levar a margem de viagem em consideração quando procuramos pelo tick magico
                for (auto offset_intervalo = util::MARGEM_DE_VIAGEM; offset_intervalo <= passo.intervalo - util::MARGEM_DE_VIAGEM; offset_intervalo += util::MARGEM_DE_VIAGEM) {
                    const auto tick_inicial = passo.fim() + offset_intervalo;
                    receita.mapear_passos_pendentes(tick_inicial);
                    if (!possui_colisoes_com_outras_receitas(receita)) {
                        // deu boa, adicionamos o tick inicial na lista de candidatos
                        candidatos[num_candidatos++] = tick_inicial;
                        // poderiamos parar aqui, mas vamos continuar procurando em outras receitas!!
                        // pode ser que encontremos um tick inicial melhor (mas é raro)
                        return util::Iter::Stop;
                    }
                }
                return util::Iter::Continue;
            });
            return util::Iter::Continue;
        },
        &receita);

    if (num_candidatos) {
        LOG("receita adicionada na fila - [estacao = ", estacao_idx, " | candidatos = ", num_candidatos, "]");
        // pegamos o menor valor da lista de candidatos, para que a receita comece o mais cedo possível
        receita.mapear_passos_pendentes(*std::min_element(candidatos.begin(), std::next(candidatos.begin(), num_candidatos)));
    } else {
        FILA_LOG("receita sendo executada imediatamente - [estacao = ", estacao_idx, "]");
        // garantimos que o bico já estará na estação antes de comecarmos
        Bico::the().viajar_para_estacao(estacao_idx);
        // ...com uma margem pra dar tempo da placa calcular tudo x)
        receita.mapear_passos_pendentes(millis() + MARGEM_CALCULO);
    }
}

// esse algoritmo é basicamente um hit-test 2d de cada passo da receita nova com cada passo das receitas já mapeadas
bool Fila::possui_colisoes_com_outras_receitas(Receita& receita_nova) {
    bool colide = false;
    receita_nova.for_each_passo_pendente([&](Receita::Passo& passo) {
        for_each_receita_mapeada(
            [&](Receita& receita) {
                receita.for_each_passo_pendente([&](Receita::Passo& passo_mapeado) {
                    // se o passo comeca antes do nosso comecar uma colisao é impossivel
                    if (passo_mapeado.comeco_abs > (passo.fim() + util::MARGEM_DE_VIAGEM))
                        // como todos os passos sao feitos em sequencia linear podemos parar de procurar nessa receita
                        // ja que todos os passos subsequentes também serão depois desse
                        return util::Iter::Stop;

                    if (passo.colide_com(passo_mapeado)) {
                        colide = true;
                        return util::Iter::Stop;
                    }
                    return util::Iter::Continue;
                });
                return util::Iter::Continue;
            },
            &receita_nova);

        if (colide)
            return util::Iter::Stop;
    });
    return colide;
}

void Fila::gerar_info(JsonObject& obj) {
    auto& estacao = Estacao::lista().at(m_estacao_ativa);
    auto& receita = *m_fila[estacao.index()];
    const auto tick = millis();

    obj["index"] = estacao.index();
    obj["status"] = static_cast<int>(estacao.status());
    {
        auto receita_obj = obj.createNestedObject("receita");
        receita_obj["id"] = receita.id();
        if (receita.possui_escaldo() && receita.escaldou())
            receita_obj["tempoAtaque"] = tick - receita.ataques().front().comeco_abs;
        if (receita.possui_escaldo())
            receita_obj["tempoEscaldo"] = tick - receita.escaldo().comeco_abs;
    }
    {
        auto passo = obj.createNestedObject("passo");
        passo["index"] = receita.passo_atual_idx();
        passo["progresso"] = receita.passo_atual().progresso;
    }
}

void Fila::cancelar_receita(Estacao::Index) {
}
}
