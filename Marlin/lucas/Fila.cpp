#include "Fila.h"
#include <lucas/Bico.h>
#include <lucas/info/info.h>
#include <src/module/motion.h>
#include <src/module/planner.h>
#include <lucas/util/ScopedGuard.h>
#include <numeric>
#include <ranges>

namespace lucas {

// TODO: mudar isso aqui tudo pra um timer de 1mhz e executar gcode injetando na fila do marlin inves de executar na hora

void Fila::tick() {
    finalizar_receitas_em_finalizacao();
    if (m_executando)
        return;

    if (m_estacao_executando != Estacao::INVALIDA) {
        if (!m_fila[m_estacao_executando].ativa) {
            LOG_ERR("estacao executando nao possui receita na fila - [estacao = ", m_estacao_executando, "]");
            m_estacao_executando = Estacao::INVALIDA;
            return;
        }

        auto& receita = m_fila[m_estacao_executando].receita;
        auto& estacao = Estacao::lista().at(m_estacao_executando);
        const auto comeco = receita.passo_atual().comeco_abs;
        if (comeco == millis()) {
            executar_passo_atual(receita, estacao);
        } else if (comeco < millis()) {
            // perdemos o passo :(
            compensar_passo_atrasado(receita, estacao);
        }
    } else {
        for_each_receita_mapeada([this](Receita& receita, size_t index) {
            const auto comeco = receita.passo_atual().comeco_abs;
            const auto tick = millis();
            // obs: a ordem desses ifs importa pq esses numeros sao unsigned! tome cuideido...
            if (comeco < tick) { // perdemos o passo :(
                compensar_passo_atrasado(receita, Estacao::lista().at(index));
                return util::Iter::Break;
            } else if (comeco - tick <= util::MARGEM_DE_VIAGEM) { // o passo está chegando...
                if (!Bico::the().esta_na_estacao(index) && !planner.busy()) {
                    m_estacao_executando = index;
                    m_executando = true;
                    Bico::the().viajar_para_estacao(index);
                    m_executando = false;
                }
                return util::Iter::Break;
            }
            return util::Iter::Continue;
        });
    }
}

void Fila::agendar_receita(JsonObjectConst json) {
    if (!json.containsKey("id") ||
        !json.containsKey("tempoNotificacao") ||
        !json.containsKey("ataques")) {
        LOG_ERR("json da receita nao possui todos os campos obrigatorios");
        return;
    }

    for (size_t i = 0; i < m_fila.size(); i++) {
        auto& estacao = Estacao::lista().at(i);
        if (estacao.status() != Estacao::Status::Livre || estacao.bloqueada())
            continue;

        if (!m_fila[i].ativa) {
            auto& receita = m_fila[i].receita;
            receita.montar_com_json(json);
            agendar_receita_para_estacao(receita, i);
            return;
        }
    }

    LOG_ERR("nao foi possivel encontrar uma estacao disponivel");
}

void Fila::agendar_receita_para_estacao(Receita& receita, size_t index) {
    auto& estacao = Estacao::lista().at(index);
    if (m_fila[index].ativa) {
        LOG_ERR("estacao ja possui uma receita na fila - [estacao = ", index, "]");
        return;
    }

    if (estacao.status() != Estacao::Status::Livre || estacao.bloqueada()) {
        LOG_ERR("tentando agendar receita para uma estacao invalida [estacao = ", index, "]");
        return;
    }

    const auto status = receita.possui_escaldo() ? Estacao::Status::AguardandoConfirmacao : Estacao::Status::AguardandoCafe;
    estacao.set_status(status, receita.id());
    adicionar_receita(index);
    LOG_IF(LogFila, "receita agendada, aguardando input - [estacao = ", index, "]");
}

void Fila::mapear_receita_da_estacao(size_t index) {
    auto& estacao = Estacao::lista().at(index);
    if (!m_fila[index].ativa) {
        LOG_ERR("estacao nao possui receita na fila - [estacao = ", index, "]");
        return;
    }

    const bool aguardando_confirmacao = estacao.status() == Estacao::Status::AguardandoConfirmacao || estacao.status() == Estacao::Status::AguardandoCafe;
    if (!aguardando_confirmacao || estacao.bloqueada()) {
        LOG_ERR("tentando mapear receita de uma estacao invalida [estacao = ", index, "]");
        return;
    }

    auto& receita = m_fila[index].receita;
    if (receita.passos_pendentes_estao_mapeados()) {
        LOG_ERR("receita ja esta mapeada - [estacao = ", index, "]");
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
        receita.mapear_passos_pendentes(melhor_tick);

        LOG_IF(LogFila, "receita mapeada - [estacao = ", estacao.index(), " | candidatos = ", num_candidatos, " | melhor_tick = ", melhor_tick, "]");
    } else {
        // garantimos que o bico já estará na estação antes de comecarmos
        m_executando = true;
        Bico::the().viajar_para_estacao(estacao);
        m_executando = false;

        m_estacao_executando = estacao.index();
        receita.mapear_passos_pendentes(millis());

        LOG_IF(LogFila, "receita mapeada - [estacao = ", estacao.index(), " | tick_inicial = ", millis(), "]");
    }
}

void Fila::executar_passo_atual(Receita& receita, Estacao& estacao) {
    LOG_IF(LogFila, "executando passo - [estacao = ", estacao.index(), " | passo = ", receita.passo_atual_idx(), " | tick = ", millis(), "]");

    const auto index = estacao.index();
    const auto comeco = receita.passo_atual().comeco_abs;
    const auto duracao_ideal = receita.passo_atual().duracao;
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

    const auto duracao_real = millis() - comeco;
    const auto erro = (duracao_ideal > duracao_real) ? (duracao_ideal - duracao_real) : (duracao_real - duracao_ideal);
    LOG_IF(LogFila, "passo acabou - [duracao = ", duracao_real, "ms | erro = ", erro, "ms]");

    if (escaldo) {
        estacao.set_status(Estacao::Status::AguardandoCafe, receita.id());
    } else if (acabou) {
        retirar_bico_do_caminho_se_necessario(receita, estacao);
        if (receita.tempo_de_finalizacao()) {
            estacao.set_status(Estacao::Status::Finalizando, receita.id());
            receita.set_inicio_tempo_finalizacao(millis());
            LOG_IF(LogFila, "receita acabou, iniciando finalizacao - [estacao = ", index, " | tempo = ", receita.tempo_de_finalizacao(), "ms]");
        } else { // isso aqui nao tem como acontecer mas caso aconteca ne ..
            estacao.set_status(Estacao::Status::Pronto, receita.id());
            remover_receita(index);
            LOG_IF(LogFila, "receita acabou, removendo da fila - [estacao = ", index, "]");
        }
    }
}

void Fila::compensar_passo_atrasado(Receita& receita, Estacao& estacao) {
    m_executando = true;
    Bico::the().viajar_para_estacao(estacao);
    m_executando = false;

    const auto comeco = receita.passo_atual().comeco_abs;
    const auto delta = millis() - comeco;

    LOG_IF(LogFila, "perdeu um tick, compensando - [estacao = ", estacao.index(), " | comeco = ", comeco, " | delta =  ", delta, "ms]");

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

    for_each_receita_mapeada([&](Receita& receita, size_t index) {
        const bool passo_min = receita.possui_escaldo() && receita.escaldou();
        const bool executou_algum_passo = receita.passo_atual_idx() > passo_min;
        if (executou_algum_passo || m_estacao_executando == index)
            // caso a receita ja tenha comecado não é remapeada
            return util::Iter::Continue;

        receita.desmapear_passos();
        receitas_para_remapear[num_receitas_para_remapear++] = index;

        return util::Iter::Continue;
    });

    for (size_t i = 0; i < num_receitas_para_remapear; ++i) {
        const auto idx = receitas_para_remapear[i];
        mapear_receita(m_fila[idx].receita, Estacao::lista().at(idx));
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

void Fila::cancelar_receita_da_estacao(size_t index) {
    if (!m_fila[index].ativa) {
        LOG_ERR("tentando cancelar receita de estacao que nao esta na fila - [estacao = ", index, "]");
        return;
    }

    remover_receita(index);
    Estacao::lista().at(index).set_status(Estacao::Status::Livre);
    LOG_IF(LogFila, "receita cancelada - [estacao = ", index, "]");

    if (index == m_estacao_executando) {
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

void Fila::adicionar_receita(size_t index) {
    if (m_num_receitas == m_fila.size()) {
        LOG_ERR("tentando adicionar receita com a fila cheia - [estacao = ", index, "]");
        return;
    }
    m_num_receitas++;
    m_fila[index].ativa = true;
}

void Fila::remover_receita(size_t index) {
    if (m_num_receitas == 0) {
        LOG_ERR("tentando remover receita com a fila vazia - [estacao = ", index, "]");
        return;
    }
    m_num_receitas--;
    m_fila[index].ativa = false;
    m_fila[index].receita.resetar();
}

void Fila::finalizar_receitas_em_finalizacao() {
    if (!m_num_receitas) [[likely]]
        return;

    for (size_t i = 0; i < m_fila.size(); ++i) {
        auto& info = m_fila[i];
        auto& receita = info.receita;
        if (!info.ativa || info.receita.passos_pendentes_estao_mapeados())
            continue;

        auto& estacao = Estacao::lista().at(i);
        if (estacao.status() == Estacao::Status::Finalizando) [[unlikely]] {
            const auto delta = millis() - receita.inicio_tempo_de_finalizacao();
            if (delta >= receita.tempo_de_finalizacao()) {
                LOG_IF(LogFila, "tempo de finalizacao acabou - [estacao = ", i, " | delta = ", delta, "ms]");
                estacao.set_status(Estacao::Status::Pronto, receita.id());
                remover_receita(i);
            }
        }
    }
}

void Fila::retirar_bico_do_caminho_se_necessario(Receita& receita, Estacao& estacao) {
    size_t num_estacoes_ativas = 0;
    for (size_t i = 0; i < m_fila.size(); ++i) {
        if (!m_fila[i].ativa)
            continue;

        auto& outra_estacao = Estacao::lista().at(i);
        auto& outra_receita = m_fila[i].receita;
        if (outra_estacao.index() == estacao.index() ||
            outra_estacao.status() == Estacao::Status::Finalizando ||
            !outra_receita.passos_pendentes_estao_mapeados())
            continue;

        ++num_estacoes_ativas;
    }

    if (num_estacoes_ativas)
        return;

    auto primeira_estacao_desbloqueada = [] {
        size_t res = Estacao::INVALIDA;
        Estacao::for_each_if(std::not_fn(&Estacao::bloqueada), [&](Estacao& estacao) {
            res = estacao.index();
            return util::Iter::Break;
        });
        return res;
    }();

    if (primeira_estacao_desbloqueada == Estacao::INVALIDA)
        // isso aqui nao é pra acontecer nunca!
        return;

    Bico::the().viajar_para_estacao(primeira_estacao_desbloqueada, -80);
}
}
