#include "Fila.h"
#include <lucas/Bico.h>
#include <lucas/info/info.h>
#include <src/module/motion.h>
#include <src/module/planner.h>
#include <lucas/util/ScopedGuard.h>
#include <numeric>
#include <ranges>

namespace lucas {
static millis_t s_tick_comeco_de_inatividade = 0;

void Fila::setup() {
    s_tick_comeco_de_inatividade = millis();
}

void Fila::tick() {
    tentar_aquecer_apos_inatividade();

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
            compensar_passo_atrasado(receita, estacao);
        }
    } else {
        for_each_receita_mapeada([this](Receita& receita, size_t index) {
            const auto comeco = receita.passo_atual().comeco_abs;
            // a ordem desses ifs importa por causa da subtracao de numeros unsigned, tome cuideido...
            if (comeco < millis()) {
                compensar_passo_atrasado(receita, Estacao::lista().at(index));
                return util::Iter::Break;
            } else if (comeco - millis() <= util::MARGEM_DE_VIAGEM) {
                // o passo está chegando...
                m_estacao_executando = index;
                LOG_IF(LogFila, "passo esta prestes a comecar - [estacao = ", index, "]");
                Bico::the().viajar_para_estacao(index);
                return util::Iter::Break;
            }
            return util::Iter::Continue;
        });
    }
}

void Fila::agendar_receita(JsonObjectConst json) {
    if (!json.containsKey("id") ||
        !json.containsKey("tempoFinalizacao") ||
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

    const auto status = receita.possui_escaldo() ? Estacao::Status::ConfirmandoEscaldo : Estacao::Status::ConfirmandoAtaques;
    estacao.set_status(status, receita.id());
    adicionar_receita(index);
    LOG_IF(LogFila, "receita agendada, aguardando confirmacao - [estacao = ", index, "]");
}

void Fila::mapear_receita_da_estacao(size_t index) {
    auto& estacao = Estacao::lista().at(index);
    if (!m_fila[index].ativa) {
        LOG_ERR("estacao nao possui receita na fila - [estacao = ", index, "]");
        return;
    }

    if (!estacao.aguardando_confirmacao() || estacao.bloqueada()) {
        LOG_ERR("tentando mapear receita de uma estacao invalida [estacao = ", index, "]");
        return;
    }

    auto& receita = m_fila[index].receita;
    if (receita.passos_pendentes_estao_mapeados()) {
        LOG_ERR("receita ja esta mapeada - [estacao = ", index, "]");
        return;
    }

    // por conta do if acima nesse momento a receita só pode estar em um de dois estados - 'ConfirmandoEscaldo' e 'ConfirmandoAtaques'
    // os próximo estados após esses são 'Escaldando' e 'FazendoCafe', respectivamente
    const auto proximo_status = int(estacao.status()) + 1;
    estacao.set_status(Estacao::Status(proximo_status), receita.id());
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
                        if (num_candidatos < candidatos.size()) [[likely]] {
                            // deu boa, adicionamos o tick inicial na lista de candidatos
                            candidatos[num_candidatos++] = tick_inicial;
                        }
                        // poderiamos parar o loop exterior aqui mas continuamos procurando em outras receitas
                        // pois talvez exista um tick inicial melhor
                        return util::Iter::Break;
                    }
                }
                return util::Iter::Continue;
            });
            return util::Iter::Continue;
        },
        &receita);

    millis_t tick_inicial = 0;
    if (num_candidatos) {
        // pegamos o menor valor da lista de candidatos, para que a receita comece o mais cedo possível
        // obs: nao tem perigo de dar deref no iterator direto aqui
        tick_inicial = *std::min_element(candidatos.begin(), std::next(candidatos.begin(), num_candidatos));
        receita.mapear_passos_pendentes(tick_inicial);
    } else {
        // se não foi achado nenhum candidato a fila está vazia
        // então a receita é executada imediatamente
        Bico::the().viajar_para_estacao(estacao);
        tick_inicial = millis();
        m_estacao_executando = estacao.index();
        receita.mapear_passos_pendentes(tick_inicial);
    }
    LOG_IF(LogFila, "receita mapeada - [estacao = ", estacao.index(), " | candidatos = ", num_candidatos, " | tick_inicial = ", tick_inicial, "]");
}

void Fila::executar_passo_atual(Receita& receita, Estacao& estacao) {
    LOG_IF(LogFila, "executando passo - [estacao = ", estacao.index(), " | passo = ", receita.passo_atual_index(), " | tick = ", millis(), "]");

    const auto& passo = receita.passo_atual();
    info::evento(
        "novoPasso",
        [&receita, &estacao](JsonObject o) {
            o["estacao"] = estacao.index();
            o["passo"] = receita.passo_atual_index();
        });

    m_estacao_executando = estacao.index();

    {
        util::FiltroUpdatesTemporario f{ Filtros::Fila }; // nada de tick()!
        receita.executar_passo_atual();
    }

    // receita foi cancelada por meios externos
    if (m_estacao_executando == Estacao::INVALIDA) [[unlikely]] {
        receita_cancelada(estacao.index());
        return;
    }

    m_estacao_executando = Estacao::INVALIDA;

    const auto duracao_real = millis() - passo.comeco_abs;
    const auto duracao_ideal = passo.duracao;
    const auto erro = (duracao_ideal > duracao_real) ? (duracao_ideal - duracao_real) : (duracao_real - duracao_ideal);
    LOG_IF(LogFila, "passo acabou - [duracao = ", duracao_real, "ms | erro = ", erro, "ms]");

    s_tick_comeco_de_inatividade = millis();

    if (estacao.status() == Estacao::Status::Escaldando) {
        estacao.set_status(Estacao::Status::ConfirmandoAtaques, receita.id());
    } else if (receita.acabou()) {
        if (receita.tempo_de_finalizacao()) {
            estacao.set_status(Estacao::Status::Finalizando, receita.id());
            receita.set_inicio_tempo_finalizacao(millis());
        } else {
            estacao.set_status(Estacao::Status::Pronto, receita.id());
            remover_receita(estacao.index());
        }
        LOG_IF(LogFila, "receita acabou, finalizando - [estacao = ", estacao.index(), "]");
    }
}

// pode acontecer do passo de uma receita nao ser executado no tick correto, pois a 'Fila::tick'
// nao é necessariamente executada a 1mhz, quando isso acontecer o passo atrasado é executado imediatamente
// e todos os outros sao compensados pelo tempo de atraso
void Fila::compensar_passo_atrasado(Receita& receita, Estacao& estacao) {
    Bico::the().viajar_para_estacao(estacao);

    const auto comeco = receita.passo_atual().comeco_abs;
    // esse delta já leva em consideração o tempo de viagem para a estacão em atraso
    const auto delta = millis() - comeco;

    LOG_IF(LogFila, "perdeu um passo, compensando - [estacao = ", estacao.index(), " | comeco = ", comeco, " | delta =  ", delta, "ms]");

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

// ao ocorrer uma mudança na fila (ex: cancelar uma receita)
// as receitas que ainda não começaram são trazidas para frente, para evitar grandes períodos onde a máquina não faz nada
void Fila::remapear_receitas_apos_mudanca_na_fila() {
    // finge que isso é um vector
    std::array<size_t, Estacao::NUM_MAX_ESTACOES> receitas_para_remapear = {};
    size_t num_receitas_para_remapear = 0;

    for_each_receita_mapeada([&](Receita& receita, size_t index) {
        const size_t passo_min = receita.possui_escaldo() && receita.escaldou();
        const bool executou_algum_passo = receita.passo_atual_index() > passo_min;
        if (executou_algum_passo || m_estacao_executando == index)
            // caso a receita ja tenha comecado não é remapeada
            return util::Iter::Continue;

        receita.desmapear_passos();
        receitas_para_remapear[num_receitas_para_remapear++] = index;

        return util::Iter::Continue;
    });

    for (auto index : receitas_para_remapear)
        mapear_receita(m_fila[index].receita, Estacao::lista().at(index));
}

// esse algoritmo é basicamente um hit-test 2d de cada passo da receita nova com cada passo das receitas já mapeadas
// é verificado se qualquer um dos passos da receita nova colide com qualquer um dos passos de qualquer uma das receitas já mapeada
bool Fila::possui_colisoes_com_outras_receitas(const Receita& receita_nova) const {
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

    if (index == m_estacao_executando) {
        m_estacao_executando = Estacao::INVALIDA;
        // 'receita_cancelada' vai ser chamada dentro da 'Fila::executar_passo_atual'
    } else {
        receita_cancelada(index);
    }
}

void Fila::remover_receitas_finalizadas() {
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

// se a fila fica um certo periodo inativa o bico é enviado para o esgoto e despeja agua por alguns segundos
// com a finalidade de esquentar a mangueira e aliviar imprecisoes na hora de comecar um café
void Fila::tentar_aquecer_apos_inatividade() const {
    constexpr millis_t tempo_de_espera_apos_execucao = 60000 * 10; // 10min
    static_assert(tempo_de_espera_apos_execucao >= 60000, "minimo de 1 minuto");

    constexpr millis_t tempo_de_despejo = 1000 * 20;                    // 20s
    constexpr float volume_a_despejar = 10 * (tempo_de_despejo / 1000); // 10 g/s

    if (s_tick_comeco_de_inatividade && millis() > s_tick_comeco_de_inatividade) {
        const auto delta = millis() - s_tick_comeco_de_inatividade;
        if (delta >= tempo_de_espera_apos_execucao) {
            const auto delta_em_min = delta / 60000;
            LOG_IF(LogFila, "aquecendo apos ", delta_em_min, "min de inatividade");

            Bico::the().viajar_para_esgoto();
            Bico::the().despejar_volume_e_aguardar(tempo_de_despejo, volume_a_despejar);

            LOG_IF(LogFila, "aquecimento finalizado");
            s_tick_comeco_de_inatividade = millis();
        }
    }
}

void Fila::receita_cancelada(size_t index) {
    LOG_IF(LogFila, "receita cancelada - [estacao = ", index, "]");
    remapear_receitas_apos_mudanca_na_fila();
}

// o conceito de uma receita "em execucao" engloba somente os despejos em sí (escaldo e ataques)
// estacões que estão aguardando input do usuário ou finalizando não possuem seus passos pendentes mapeados
// consequentemente, não são consideradas como "em execucao"
size_t Fila::numero_de_receitas_em_execucao() const {
    size_t res = 0;
    for_each_receita_mapeada([&res](const auto&) {
        ++res;
        return util::Iter::Continue;
    });
    return res;
}

void Fila::gerar_informacoes_da_fila() const {
    info::evento(
        "infoEstacoes",
        [this](JsonObject o) {
            if (m_estacao_executando != Estacao::INVALIDA)
                o["estacaoAtiva"] = m_estacao_executando;

            auto arr = o.createNestedArray("info");
            for_each_receita([this, &arr](const Receita& receita, size_t index) {
                auto obj = arr.createNestedObject();
                const auto tick = millis();
                const auto& estacao = Estacao::lista().at(index);

                obj["estacao"] = estacao.index();
                obj["status"] = int(estacao.status());
                {
                    auto receita_obj = obj.createNestedObject("infoReceita");
                    receita_obj["receitaId"] = receita.id();

                    auto timer_valido = [tick](millis_t timer) {
                        return timer && tick > timer;
                    };

                    const auto& primeiro_passo = receita.primeiro_passo();
                    if (timer_valido(primeiro_passo.comeco_abs))
                        receita_obj["tempoTotalReceita"] = tick - receita.primeiro_passo().comeco_abs;

                    const auto& primeiro_ataque = receita.primeiro_ataque();
                    if (timer_valido(primeiro_ataque.comeco_abs))
                        receita_obj["tempoTotalAtaques"] = tick - primeiro_ataque.comeco_abs;

                    if (estacao.status() == Estacao::Status::Finalizando)
                        if (timer_valido(receita.inicio_tempo_de_finalizacao()))
                            receita_obj["progressoFinalizacao"] = millis() - receita.inicio_tempo_de_finalizacao();
                }
                {
                    auto passo_obj = obj.createNestedObject("infoPasso");
                    if (!receita.passo_atual().comeco_abs)
                        return util::Iter::Continue;

                    // se o passo ainda não comecou porém está mapeado ele pode estar ou no intervalo do ataque passado ou na fila para começar...
                    if (tick < receita.passo_atual().comeco_abs) {
                        // ...se é o escaldo ou o primeiro ataque não existe um "passo anterior", ou seja, não tem um intervalo - só pode estar na fila
                        // nesse caso nada é enviado para o app
                        if (receita.passo_atual_index() <= size_t(receita.possui_escaldo()))
                            return util::Iter::Continue;

                        // ...caso contrario, do segundo ataque pra frente, estamos no intervalo do passo anterior
                        const auto index_ultimo_passo = receita.passo_atual_index() - 1;
                        const auto& ultimo_passo = receita.passo(index_ultimo_passo);
                        passo_obj["index"] = index_ultimo_passo;
                        passo_obj["progressoIntervalo"] = tick - ultimo_passo.fim();
                    } else {
                        passo_obj["index"] = receita.passo_atual_index();
                        passo_obj["progressoPasso"] = tick - receita.passo_atual().comeco_abs;
                    }
                }

                return util::Iter::Continue;
            });
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
}
}
