#include "Fila.h"
#include <lucas/Bico.h>
#include <lucas/info/info.h>
#include <src/module/motion.h>
#include <src/module/planner.h>
#include <lucas/util/ScopedGuard.h>
#include <numeric>
#include <ranges>

namespace lucas {
#define FILA_LOG(...) LOG("", "FILA: ", __VA_ARGS__);

// TODO: mudar isso aqui tudo pra um timer de 1mhz e executar gcode injetando na fila do marlin inves de executar na hora

void Fila::tick() {
    finalizar_receitas_em_notificacao();
    if (m_executando)
        return;

    if (m_estacao_executando != Estacao::INVALIDA) {
        if (!m_fila[m_estacao_executando]) {
            FILA_LOG("ERRO - estacao executando nao possui receita na fila - [estacao = ", m_estacao_executando, "]");
            m_estacao_executando = Estacao::INVALIDA;
            return;
        }

        auto& receita = *m_fila[m_estacao_executando];
        auto& estacao = Estacao::lista().at(m_estacao_executando);
        const auto comeco = receita.passo_atual().comeco_abs;
        if (comeco == millis()) {
            executar_passo_atual(receita, estacao);
        } else if (comeco < millis()) {
            // perdemos o passo :(
            compensar_passo_atrasado(receita, estacao);
        }
    } else {
        for_each_receita_mapeada([this](Receita& receita, Estacao::Index estacao_idx) {
            const auto comeco = receita.passo_atual().comeco_abs;
            const auto tick = millis();
            // obs: a ordem desses ifs importa pq esses numeros sao unsigned! tome cuideido...
            if (comeco < tick) { // perdemos o passo :(
                compensar_passo_atrasado(receita, Estacao::lista().at(estacao_idx));
                return util::Iter::Break;
            } else if (comeco - tick <= util::MARGEM_DE_VIAGEM) { // o passo está chegando...
                if (!Bico::the().esta_na_estacao(estacao_idx) && !planner.busy()) {
                    m_estacao_executando = estacao_idx;
                    m_executando = true;
                    Bico::the().viajar_para_estacao(estacao_idx);
                    m_executando = false;
                }
                return util::Iter::Break;
            }
            return util::Iter::Continue;
        });
    }
}

void Fila::agendar_receita(std::unique_ptr<Receita> receita) {
    for (size_t i = 0; i < m_fila.size(); i++) {
        auto& estacao = Estacao::lista().at(i);
        if (estacao.status() != Estacao::Status::FREE || estacao.bloqueada())
            continue;

        if (!m_fila[i]) {
            agendar_receita_para_estacao(std::move(receita), i);
            return;
        }
    }

    FILA_LOG("ERRO - nao foi possivel encontrar uma estacao disponivel");
}

void Fila::agendar_receita_para_estacao(std::unique_ptr<Receita> receita, Estacao::Index estacao_idx) {
    auto& estacao = Estacao::lista().at(estacao_idx);
    if (m_fila[estacao_idx]) {
        FILA_LOG("ERRO - estacao ja possui uma receita na fila - [estacao = ", estacao_idx, "]");
        return;
    }

    if (estacao.status() != Estacao::Status::FREE || estacao.bloqueada()) {
        FILA_LOG("ERRO - tentando agendar receita para uma estacao invalida [estacao = ", estacao_idx, "]");
        return;
    }

    estacao.set_status(Estacao::Status::WAITING_START, receita->id());
    adicionar_receita(std::move(receita), estacao_idx);
    FILA_LOG("receita agendada, aguardando input - [estacao = ", estacao_idx, "]");
}

void Fila::mapear_receita_da_estacao(Estacao::Index estacao_idx) {
    auto& estacao = Estacao::lista().at(estacao_idx);
    if (!m_fila[estacao_idx]) {
        FILA_LOG("ERRO - estacao nao possui receita na fila - [estacao = ", estacao_idx, "]");
        return;
    }

    if (!estacao.aguardando_input() || estacao.bloqueada()) {
        FILA_LOG("ERRO - tentando mapear receita de uma estacao invalida [estacao = ", estacao_idx, "]");
        return;
    }

    auto& receita = *m_fila[estacao_idx];
    if (receita.passos_pendentes_estao_mapeados()) {
        FILA_LOG("ERRO - receita ja esta mapeada - [estacao = ", estacao_idx, "]");
        return;
    }

    const auto status_atual = int(estacao.status());
    estacao.set_status(Estacao::Status(status_atual + 1), receita.id());
    mapear_receita(receita, estacao);
}

void Fila::mapear_receita(Receita& receita, Estacao& estacao) {
    // finge que isso é um vector
    std::array<millis_t, Estacao::NUM_MAX_ESTACOES> candidatos = {};
    size_t num_candidatos = 0;

    // tentamos encontrar um tick inicial que não causa colisões com nenhuma das outras receita
    // esse processo é feito de forma bruta, homem das cavernas, mas como o número de receitas/passos é pequeno, não chega a ser um problema
    for_each_receita_mapeada(
        [&](const Receita& receita_mapeada) {
            receita_mapeada.for_each_passo_pendente([&](const Receita::Passo& passo) {
                // temos que sempre levar a margem de viagem em consideração quando procuramos pelo tick magico
                for (auto offset_intervalo = util::MARGEM_DE_VIAGEM; offset_intervalo <= passo.intervalo - util::MARGEM_DE_VIAGEM; offset_intervalo += util::MARGEM_DE_VIAGEM) {
                    const auto tick_inicial = passo.fim() + offset_intervalo;
                    receita.mapear_passos_pendentes(tick_inicial);
                    if (!possui_colisoes_com_outras_receitas(receita)) {
                        if (num_candidatos < candidatos.size()) {
                            // deu boa, adicionamos o tick inicial na lista de candidatos
                            candidatos[num_candidatos++] = tick_inicial;
                        }
                        // poderiamos parar o loop exterior aqui mas continuamos procurando em outras receitas pois talvez exista um tick inicial melhor
                        return util::Iter::Break;
                    }
                }
                return util::Iter::Continue;
            });
            return util::Iter::Continue;
        },
        &receita);

    if (num_candidatos) {
        // pegamos o menor valor da lista de candidatos, para que a receita comece o mais cedo possível
        // obs: nao tem perigo de dar deref no iterator direto aqui
        const auto melhor_tick = *std::min_element(candidatos.begin(), std::next(candidatos.begin(), num_candidatos));
        FILA_LOG("receita mapeada - [estacao = ", estacao.index(), " | candidatos = ", num_candidatos, " | melhor_tick = ", melhor_tick, "]");
        receita.mapear_passos_pendentes(melhor_tick);
    } else {
        // garantimos que o bico já estará na estação antes de comecarmos
        m_executando = true;
        Bico::the().viajar_para_estacao(estacao);
        m_executando = false;

        m_estacao_executando = estacao.index();
        receita.mapear_passos_pendentes(millis());

        FILA_LOG("receita mapeada - [estacao = ", estacao.index(), " | tick_inicial = ", millis(), "]");
    }
}

void Fila::executar_passo_atual(Receita& receita, Estacao& estacao) {
    FILA_LOG("executando passo - [tick = ", millis(), " | estacao = ", estacao.index(), " | passo = ", receita.passo_atual_idx(), "]");

    const auto index = estacao.index();
    const auto comeco = receita.passo_atual().comeco_abs;
    const bool escaldo = receita.possui_escaldo() && !receita.escaldou();

    info::evento(NovoPasso{
        .estacao = index,
        .novo_passo = receita.passo_atual_idx(),
        .receita_id = receita.id() });

    m_estacao_executando = index;
    m_executando = true;
    const bool acabou = receita.executar_passo_atual();
    m_executando = false;

    // receita foi cancelada por meios externos
    if (m_estacao_executando == Estacao::INVALIDA) [[unlikely]] {
        tentar_remapear_receitas_apos_cancelamento();
        return;
    }

    m_estacao_executando = Estacao::INVALIDA;

    FILA_LOG("passo acabou - [duracao = ", millis() - comeco, "]");

    if (escaldo)
        estacao.set_status(Estacao::Status::INITIALIZE_COFFEE, receita.id());

    if (acabou) {
        if (receita.tempo_de_notificacao()) {
            estacao.set_status(Estacao::Status::NOTIFICATION_TIME, receita.id());
            receita.set_inicio_tempo_notificacao(millis());
            FILA_LOG("receita acabou, iniciando notificacao - [estacao = ", index, " | tempo de notificacao = ", receita.tempo_de_notificacao(), "]");
        } else { // isso aqui nao tem como acontecer mas caso aconteca ne ..
            estacao.set_status(Estacao::Status::IS_READY, receita.id());
            remover_receita(index);
            FILA_LOG("receita acabou, removendo da fila - [estacao = ", index, "]");
        }
    }
}

void Fila::compensar_passo_atrasado(Receita& receita, Estacao& estacao) {
    m_executando = true;
    Bico::the().viajar_para_estacao(estacao);
    m_executando = false;

    const auto comeco = receita.passo_atual().comeco_abs;
    const auto delta = millis() - comeco;

    FILA_LOG("perdemos um tick, compensando - [estacao = ", estacao.index(), " | comeco = ", comeco, " | delta =  ", delta, "ms]");

    receita.mapear_passos_pendentes(millis());
    executar_passo_atual(receita, estacao);

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

void Fila::tentar_remapear_receitas_apos_cancelamento() {
    if (m_num_receitas <= 1)
        return;

    // finge que isso é um vector
    std::array<size_t, Estacao::NUM_MAX_ESTACOES> receitas_para_remapear = {};
    size_t num_receitas_para_remapear = 0;

    for_each_receita_mapeada([&](Receita& receita, size_t i) {
        const bool passo_min = receita.possui_escaldo() && receita.escaldou();
        const bool executou_algum_passo = receita.passo_atual_idx() > passo_min;
        if (executou_algum_passo || m_estacao_executando == i)
            // caso a receita ja tenha comecado não mexemos nos seus intervalos
            return util::Iter::Continue;

        receita.desmapear_passos();
        receitas_para_remapear[num_receitas_para_remapear++] = i;

        return util::Iter::Continue;
    });

    for (size_t i = 0; i < num_receitas_para_remapear; ++i) {
        const auto idx = receitas_para_remapear[i];
        mapear_receita(*m_fila[idx], Estacao::lista().at(idx));
    }

    return;
}

// esse algoritmo é basicamente um hit-test 2d de cada passo da receita nova com cada passo das receitas já mapeadas
bool Fila::possui_colisoes_com_outras_receitas(const Receita& receita_nova) {
    bool colide = false;
    receita_nova.for_each_passo_pendente([&](const Receita::Passo& passo_novo) {
        for_each_receita_mapeada(
            [&](const Receita& receita_mapeada) {
                receita_mapeada.for_each_passo_pendente([&](const Receita::Passo& passo_mapeado) {
                    // se o passo comeca antes do nosso comecar uma colisao é impossivel
                    if (passo_mapeado.comeco_abs > (passo_novo.fim() + util::MARGEM_DE_VIAGEM)) {
                        // como todos os passos sao feitos em sequencia linear podemos parar de procurar nessa receita
                        // ja que todos os passos subsequentes também serão depois desse
                        return util::Iter::Break;
                    }
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

void Fila::cancelar_receita_da_estacao(Estacao::Index estacao_idx) {
    if (!m_fila[estacao_idx]) {
        FILA_LOG("ERRO - tentando cancelar receita de estacao que nao esta na fila - [estacao = ", estacao_idx, "]");
        return;
    }

    FILA_LOG("cancelando receita - [estacao = ", estacao_idx, "]");

    remover_receita(estacao_idx);
    Estacao::lista().at(estacao_idx).set_status(Estacao::Status::FREE);

    if (estacao_idx == m_estacao_executando) {
        m_estacao_executando = Estacao::INVALIDA;
        // melhor desligar por garantia
        Bico::the().desligar();
        // caso a receita cancelada seja a que esta sendo executada as outras só serao remapeadas após retornar de "executar_passo_atual"
        return;
    }

    tentar_remapear_receitas_apos_cancelamento();
}

void Fila::printar_informacoes() {
    info::evento(InfoEstacoes{ .fila = *this });
}

void Fila::InfoEstacoes::gerar_json_impl(JsonObject o) const {
    auto arr = o.createNestedArray("info");
    fila.for_each_receita_mapeada([this, &arr](Receita& receita, size_t index) {
        auto obj = arr.createNestedObject();
        const auto tick = millis();
        const auto& estacao = Estacao::lista().at(index);

        obj["estacao"] = estacao.index();
        obj["status"] = static_cast<int>(estacao.status());
        {
            auto receita_obj = obj.createNestedObject("infoReceita");
            receita_obj["id"] = receita.id();
            if (receita.ataques().front().comeco_abs) {
                if (tick < receita.ataques().front().comeco_abs)
                    receita_obj["tempoTotalAtaques"] = 0;
                else
                    receita_obj["tempoTotalAtaques"] = tick - receita.ataques().front().comeco_abs;
            }
            if (tick < receita.primeiro_passo().comeco_abs)
                receita_obj["tempoTotalReceita"] = 0;
            else
                receita_obj["tempoTotalReceita"] = tick - receita.primeiro_passo().comeco_abs;
        }
        {
            auto passo_obj = obj.createNestedObject("infoPasso");
            passo_obj["index"] = receita.passo_atual_idx();
            if (tick < receita.passo_atual().comeco_abs)
                passo_obj["progresso"] = 0;
            else
                passo_obj["progresso"] = tick - receita.passo_atual().comeco_abs;
        }

        return util::Iter::Continue;
    });
}

void Fila::adicionar_receita(std::unique_ptr<Receita> receita, Estacao::Index estacao_idx) {
    if (m_num_receitas == m_fila.size()) {
        FILA_LOG("ERRO - tentando adicionar receita com a fila cheia - [estacao = ", estacao_idx, "]");
        return;
    }
    m_num_receitas++;
    m_fila[estacao_idx] = std::move(receita);
}

void Fila::remover_receita(Estacao::Index estacao_idx) {
    if (m_num_receitas == 0) {
        FILA_LOG("ERRO - tentando remover receita com a fila vazia - [estacao = ", estacao_idx, "]");
        return;
    }
    m_num_receitas--;
    m_fila[estacao_idx] = nullptr;
}

void Fila::finalizar_receitas_em_notificacao() {
    if (!m_num_receitas) [[likely]]
        return;

    for (size_t i = 0; i < m_fila.size(); ++i) {
        auto& receita = m_fila[i];
        if (!receita || receita->passos_pendentes_estao_mapeados())
            continue;

        auto& estacao = Estacao::lista().at(i);
        if (estacao.status() == Estacao::Status::NOTIFICATION_TIME) [[unlikely]] {
            const auto delta = millis() - receita->inicio_tempo_de_notificacao();
            if (delta >= receita->tempo_de_notificacao()) {
                FILA_LOG("tempo de finalizacao acabou - [estacao = ", i, " | delta = ", delta, "ms]");
                estacao.set_status(Estacao::Status::FREE, receita->id());
                remover_receita(i);
            }
        }
    }
}
}
