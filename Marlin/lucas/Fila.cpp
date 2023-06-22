#include "Fila.h"
#include <lucas/Bico.h>
#include <src/module/planner.h>
#include <numeric>
#include <ranges>

namespace lucas {
#define FILA_LOG(...) LOG("", "FILA: ", __VA_ARGS__);

// 1s de "margem de erro" pra dar tempo da placa calcular tudo
constexpr auto MARGEM_CALCULO = 1000;

void Fila::tick() {
    finalizar_receitas_em_notificacao();

    if (m_ocupado)
        return;

    if (m_estacao_ativa != Estacao::INVALIDA) {
        if (!m_fila[m_estacao_ativa].valida()) {
            FILA_LOG("ERRO - estacao ativa nao esta na fila - [estacao = ", m_estacao_ativa, "]");
            m_estacao_ativa = Estacao::INVALIDA;
        }
        return;
    }

    for_each_receita_mapeada([this](Receita& receita, Estacao::Index estacao_idx) {
        auto& estacao = Estacao::lista().at(estacao_idx);
        const auto comeco = receita.passo_atual().comeco_abs;
        // - aviso - a ordem desses ifs importa! tome cuideido...

        const auto tick = millis();
        // o passo chegou!
        if (comeco == tick) {
            FILA_LOG("executando passo - [tick = ", tick, " | estacao = ", estacao_idx, " | passo = ", receita.passo_atual_idx(), "]");

            m_estacao_ativa = estacao_idx;
            if (receita.executar_passo() == Receita::Acabou::Sim) {
                if (receita.tempo_de_notificacao()) {
                    estacao.set_status(Estacao::Status::NOTIFICATION_TIME);
                    receita.set_inicio_tempo_notificacao(tick);
                    FILA_LOG("receita acabou, iniciando notificacao - [estacao = ", estacao_idx, " | tempo de notificacao = ", receita.tempo_de_notificacao());
                } else {
                    estacao.set_status(Estacao::Status::IS_READY);
                    remover_receita(estacao_idx);
                    FILA_LOG("receita acabou, removendo da fila - [estacao = ", estacao_idx, "]");
                }
            }
            m_estacao_ativa = Estacao::INVALIDA;
            return util::Iter::Break;
        }
        // perdemos o passo :(
        // isso é um caso raro que, em teoria, nunca deve acontecer
        // mas se acontecer, vamos tentar recuperar o mais rápido possível
        else if (comeco < tick) {
            // primeiro viajamos para a estacao atrasada e ocupamos a fila para nao ter perigo de comecarmos alguma outra receita nesse meio tempo
            m_ocupado = true;
            Bico::the().viajar_para_estacao(estacao_idx);
            m_ocupado = false;

            const auto tick_inicial = tick + MARGEM_CALCULO;
            const auto delta = tick_inicial - comeco;
            FILA_LOG("ERRO - perdemos um tick, deslocando todas as receitas - [estacao = ", estacao_idx, " | delta =  ", delta, "ms]");
            receita.mapear_passos_pendentes(tick_inicial);

            for_each_receita_mapeada(
                [delta](Receita& outra_receita) {
                    outra_receita.for_each_passo_pendente([delta](Receita::Passo& passo) {
                        // e adicionamos um offset em todos os passos restantes
                        // é melhor atrasarmos todas as receitas um pouquinho do que atrasarmos *muito* uma unica receita
                        passo.comeco_abs += delta;
                        return util::Iter::Continue;
                    });
                    return util::Iter::Continue;
                },
                &receita);
        }
        // o passo está chegando...
        else if (comeco - tick <= util::MARGEM_DE_VIAGEM) {
            if (!Bico::the().esta_na_estacao(estacao_idx) && !planner.busy())
                // colocamos o bico na estacao antes do passo chegar
                Bico::the().viajar_para_estacao(estacao_idx);
        }
        return util::Iter::Continue;
    });
}

void Fila::agendar_receita(Estacao::Index estacao_idx, std::unique_ptr<Receita> receita_ptr) {
    auto& estacao = Estacao::lista().at(estacao_idx);
    if (estacao.status() != Estacao::Status::FREE) {
        FILA_LOG("ERRO - tentando agendar receita para uma estacao que nao esta livre - [estacao = ", estacao_idx, "]");
        return;
    }

    if (estacao.bloqueada()) {
        FILA_LOG("ERRO - tentando agendar receita para uma estacao bloqueada - [estacao = ", estacao_idx, "]");
        return;
    }

    if (m_fila[estacao_idx].valida()) {
        FILA_LOG("ERRO - estacao ja possui uma receita na fila - [estacao = ", estacao_idx, "]");
        return;
    }

    adicionar_receita(std::move(receita_ptr), estacao_idx);
    FILA_LOG("receita agendada, aguardando input - [estacao = ", estacao_idx, "]");
    estacao.set_status(Estacao::Status::WAITING_START);
}

void Fila::mapear_receita(Estacao::Index estacao_idx) {
    auto& estacao = Estacao::lista().at(estacao_idx);
    if (estacao.status() != Estacao::Status::WAITING_START && estacao.status() != Estacao::Status::INITIALIZE_COFFEE) {
        FILA_LOG("ERRO - tentando mapear receita para uma estacao que nao esta aguardando input - [estacao = ", estacao_idx, "]");
        return;
    }

    if (estacao.bloqueada()) {
        FILA_LOG("ERRO - tentando mapear a receita para uma estacao bloqueada - [estacao = ", estacao_idx, "]");
        return;
    }

    if (!m_fila[estacao_idx].valida()) {
        FILA_LOG("ERRO - estacao nao possui receita na fila - [estacao = ", estacao_idx, "]");
        return;
    }

    auto& receita = *m_fila[estacao_idx].receita;
    if (receita.passos_pendentes_estao_mapeados()) {
        FILA_LOG("ERRO - receita ja esta mapeada - [estacao = ", estacao_idx, "]");
        return;
    }

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
                        // poderiamos parar aqui, mas vamos continuar procurando em outras receitas, pode ser que encontremos um tick inicial melhor
                        return util::Iter::Break;
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
        // obs: nao tem perigo de dar deref no iterator direto aqui
        const auto melhor_tick = *std::min_element(candidatos.begin(), std::next(candidatos.begin(), num_candidatos));
        receita.mapear_passos_pendentes(melhor_tick);
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
    receita_nova.for_each_passo_pendente([&](Receita::Passo& passo_novo) {
        for_each_receita_mapeada(
            [&](Receita& receita_mapeada) {
                receita_mapeada.for_each_passo_pendente([&](Receita::Passo& passo_mapeado) {
                    // se o passo comeca antes do nosso comecar uma colisao é impossivel
                    if (passo_mapeado.comeco_abs > (passo_novo.fim() + util::MARGEM_DE_VIAGEM))
                        // como todos os passos sao feitos em sequencia linear podemos parar de procurar nessa receita
                        // ja que todos os passos subsequentes também serão depois desse
                        return util::Iter::Break;

                    colide = passo_novo.colide_com(passo_mapeado);
                    return colide ? util::Iter::Break : util::Iter::Continue;
                });
                return colide ? util::Iter::Break : util::Iter::Continue;
            },
            &receita_nova);
        return colide ? util::Iter::Break : util::Iter::Continue;
    });
    return colide;
}

void Fila::cancelar_receita(Estacao::Index estacao_idx) {
    remover_receita(estacao_idx);
    if (estacao_idx == m_estacao_ativa) {
        m_estacao_ativa = Estacao::INVALIDA;
        // desligamos o bico para evitar vazamentos
        Bico::the().desligar(millis());
        // limpamos o planejador e centralizamos o bico na estacao que estava
        planner.clear_block_buffer();
        Bico::the().viajar_para_estacao(estacao_idx);
    }
}

void Fila::gerar_info(millis_t tick, JsonObject obj) const {
    const auto& estacao = Estacao::lista().at(m_estacao_ativa);
    if (!m_fila[estacao.index()].valida()) {
        FILA_LOG("ERRO - falha ao gerar info, estacao ativa nao possui receita na fila - [estacao = ", estacao.index(), "]");
        return;
    }

    auto& receita = *m_fila[estacao.index()].receita;
    receita.passo_atual().progresso = tick - receita.passo_atual().comeco_abs;

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

void Fila::remover_receita(Estacao::Index estacao_idx) {
    m_num_receitas--;
    m_fila[estacao_idx].inativa = true;
    m_fila[estacao_idx].receita = nullptr;
}

void Fila::adicionar_receita(std::unique_ptr<Receita> receita, Estacao::Index estacao_idx) {
    m_num_receitas++;
    m_fila[estacao_idx].inativa = false;
    m_fila[estacao_idx].receita = std::move(receita);
}

void Fila::finalizar_receitas_em_notificacao() {
    for_each_receita_mapeada([this](Receita& receita, Estacao::Index estacao_idx) {
        auto& estacao = Estacao::lista().at(estacao_idx);
        if (estacao.status() == Estacao::Status::NOTIFICATION_TIME) {
            if (const auto tick = millis(); tick - receita.inicio_tempo_de_notificacao() >= receita.tempo_de_notificacao()) {
                FILA_LOG("tempo de finalizacao acabou - [estacao = ", estacao_idx, " | delta = ", tick - receita.inicio_tempo_de_notificacao(), "ms]");
                estacao.set_status(Estacao::Status::IS_READY);
                remover_receita(estacao_idx);
            }
        }
        return util::Iter::Continue;
    });
}
}
