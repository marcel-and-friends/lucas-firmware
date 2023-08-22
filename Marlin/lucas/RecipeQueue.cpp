#include "RecipeQueue.h"
#include <lucas/Spout.h>
#include <lucas/info/info.h>
#include <src/module/motion.h>
#include <src/module/planner.h>
#include <lucas/util/ScopedGuard.h>
#include <lucas/util/StaticVector.h>
#include <numeric>
#include <ranges>

namespace lucas {
static millis_t s_tick_beginning_inactivity = 0;

void RecipeQueue::tick() {
    try_heating_hose_after_inactivity();

    if (m_recipe_in_execution != Station::INVALID) {
        if (not m_queue[m_recipe_in_execution].active) {
            LOG_ERR("station executando nao possui recipe na fila - [station = ", m_recipe_in_execution, "]");
            m_recipe_in_execution = Station::INVALID;
            return;
        }

        auto& recipe = m_queue[m_recipe_in_execution].recipe;
        auto& station = Station::list().at(m_recipe_in_execution);
        auto const starting_tick = recipe.current_step().starting_tick;
        if (starting_tick == millis()) {
            execute_current_step(recipe, station);
        } else if (starting_tick < millis()) {
            compensate_for_missed_step(recipe, station);
        }
    } else {
        for_each_mapped_recipe([this](Recipe& recipe, size_t index) {
            const auto starting_tick = recipe.current_step().starting_tick;
            // a ordem desses ifs importa por causa da subtracao de numeros unsigned, tome cuideido...
            if (starting_tick < millis()) {
                compensate_for_missed_step(recipe, Station::list().at(index));
                return util::Iter::Break;
            } else if (starting_tick - millis() <= util::TRAVEL_MARGIN) {
                // o passo está chegando...
                m_recipe_in_execution = index;
                LOG_IF(LogQueue, "passo esta prestes a comecar - [station = ", index, "]");
                Spout::the().travel_to_station(index);
                return util::Iter::Break;
            }
            return util::Iter::Continue;
        });
    }
}

void RecipeQueue::schedule_recipe(JsonObjectConst recipe_json) {
    if (not recipe_json.containsKey("id") or
        not recipe_json.containsKey("tempoFinalizacao") or
        not recipe_json.containsKey("ataques")) {
        LOG_ERR("json da recipe nao possui todos os campos obrigatorios");
        return;
    }

    for (size_t i = 0; i < m_queue.size(); i++) {
        auto& station = Station::list().at(i);
        if (station.status() != Station::Status::Free or station.blocked())
            continue;

        auto& info = m_queue[i];
        if (not info.active) {
            info.recipe.build_from_json(recipe_json);
            schedule_recipe_for_station(info.recipe, i);
            return;
        }
    }

    LOG_ERR("nao foi possivel encontrar uma station disponivel");
}

void RecipeQueue::schedule_recipe_for_station(Recipe& recipe, size_t index) {
    auto& station = Station::list().at(index);
    if (m_queue[index].active) {
        LOG_ERR("station ja possui uma recipe na fila - [station = ", index, "]");
        return;
    }

    if (station.status() != Station::Status::Free or station.blocked()) {
        LOG_ERR("tentando agendar recipe para uma station invalida [station = ", index, "]");
        return;
    }

    auto const status = recipe.has_scalding_step() ? Station::Status::ConfirmingScald : Station::Status::ConfirmingAttacks;
    station.set_status(status, recipe.id());
    add_recipe(index);
    LOG_IF(LogQueue, "recipe agendada, aguardando confirmacao - [station = ", index, "]");
}

void RecipeQueue::map_station_recipe(size_t index) {
    auto& station = Station::list().at(index);
    if (not m_queue[index].active) {
        LOG_ERR("station nao possui recipe na fila - [station = ", index, "]");
        return;
    }

    if (not station.waiting_user_confirmation() or station.blocked()) {
        LOG_ERR("tentando mapear recipe de uma station invalida [station = ", index, "]");
        return;
    }

    auto& recipe = m_queue[index].recipe;
    if (recipe.remaining_steps_are_mapped()) {
        LOG_ERR("recipe ja esta mapeada - [station = ", index, "]");
        return;
    }

    // por conta do if acima nesse momento a recipe só pode estar em um de dois estados - 'ConfirmingScald' e 'ConfirmingAttacks'
    // os próximo estados após esses são 'Scalding' e 'Attacking', respectivamente
    auto const next_status = int(station.status()) + 1;
    station.set_status(Station::Status(next_status), recipe.id());
    map_recipe(recipe, station);
}

void RecipeQueue::map_recipe(Recipe& recipe, Station& station) {
    util::StaticVector<millis_t, Station::MAXIMUM_STATIONS> candidates = {};

    // tentamos encontrar um tick inicial que não causa colisões com nenhuma das outras recipe
    // esse processo é feito de forma bruta, homem das cavernas, mas como o número de receitas/passos é pequeno, não chega a ser um problema
    for_each_mapped_recipe(
        [&](Recipe const& mapped_recipe) {
            mapped_recipe.for_each_remaining_step([&](const Recipe::Step& passo) {
                // temos que sempre levar a margem de viagem em consideração quando procuramos pelo tick magico
                for (auto interval_offset = util::TRAVEL_MARGIN; interval_offset <= passo.interval - util::TRAVEL_MARGIN; interval_offset += util::TRAVEL_MARGIN) {
                    const auto starting_tick = passo.ending_tick() + interval_offset;
                    recipe.map_remaining_steps(starting_tick);
                    if (not collides_with_other_recipes(recipe)) {
                        // deu boa, adicionamos o tick inicial na list de candidates
                        candidates.push_back(starting_tick);
                        // poderiamos parar o loop exterior aqui mas continuamos procurando em outras receitas
                        // pois talvez exista um tick inicial melhor
                        return util::Iter::Break;
                    }
                }
                return util::Iter::Continue;
            });
            return util::Iter::Continue;
        },
        &recipe);

    millis_t first_step_tick = 0;
    if (not candidates.is_empty()) {
        // pegamos o menor valor da list de candidates, para que a recipe comece o mais cedo possível
        // obs: nao tem perigo de dar deref no iterator direto aqui
        first_step_tick = *std::min_element(candidates.begin(), candidates.end());
        recipe.map_remaining_steps(first_step_tick);
    } else {
        // se não foi achado nenhum candidato a fila está vazia
        // então a recipe é executada imediatamente
        Spout::the().travel_to_station(station);
        first_step_tick = millis();
        m_recipe_in_execution = station.index();
        recipe.map_remaining_steps(first_step_tick);
    }
    LOG_IF(LogQueue, "recipe mapeada - [station = ", station.index(), " | candidates = ", candidates.size(), " | tick_inicial = ", first_step_tick, "]");
}

static bool valid_timer(millis_t timer, millis_t tick = millis()) {
    return timer and tick >= timer;
};

void RecipeQueue::execute_current_step(Recipe& recipe, Station& station) {
    LOG_IF(LogQueue, "executando passo - [station = ", station.index(), " | passo = ", recipe.current_step_index(), " | tick = ", millis(), "]");

    auto const& current_step = recipe.current_step();
    const size_t current_step_index = recipe.current_step_index();
    auto dispatch_step_event = [](size_t station, size_t step, millis_t first_step_tick, bool ending) {
        info::event(
            "eventoPasso",
            [&](JsonObject o) {
                o["estacao"] = station;
                o["passo"] = step;

                const auto tick = millis();
                if (valid_timer(first_step_tick, tick))
                    o["tempoTotalAtaques"] = tick - first_step_tick;

                if (ending)
                    o["fim"] = true;
            });
    };

    {
        dispatch_step_event(station.index(), current_step_index, recipe.first_attack().starting_tick, false);

        m_recipe_in_execution = station.index();

        {
            core::TemporaryFilter f{ Filters::RecipeQueue }; // nada de tick()!
            recipe.execute_current_step();
        }

        // recipe foi cancelada por meios externos
        if (m_recipe_in_execution == Station::INVALID) [[unlikely]] {
            recipe_was_cancelled(station.index());
            return;
        }

        m_recipe_in_execution = Station::INVALID;

        dispatch_step_event(station.index(), current_step_index, recipe.first_attack().starting_tick, true);
    }

    auto const ideal_duration = current_step.duration;
    auto const actual_duration = millis() - current_step.starting_tick;
    auto const error = (ideal_duration > actual_duration) ? (ideal_duration - actual_duration) : (actual_duration - ideal_duration);
    LOG_IF(LogQueue, "passo acabou - [duracao = ", actual_duration, "ms | error = ", error, "ms]");

    s_tick_beginning_inactivity = millis();

    if (station.status() == Station::Status::Scalding) {
        station.set_status(Station::Status::ConfirmingAttacks, recipe.id());
    } else if (recipe.finished()) {
        if (recipe.finalization_duration()) {
            station.set_status(Station::Status::Finalizing, recipe.id());
            recipe.set_start_finalization_duration(millis());
        } else {
            station.set_status(Station::Status::Ready, recipe.id());
            remove_recipe(station.index());
        }
        LOG_IF(LogQueue, "recipe acabou, finalizando - [station = ", station.index(), "]");
    }
}

// pode acontecer do passo de uma recipe nao ser executado no tick correto, pois a 'RecipeQueue::tick'
// nao é necessariamente executada a 1mhz, quando isso acontecer o passo atrasado é executado imediatamente
// e todos os outros sao compensados pelo tempo de atraso
void RecipeQueue::compensate_for_missed_step(Recipe& recipe, Station& station) {
    Spout::the().travel_to_station(station);

    auto const starting_tick = recipe.current_step().starting_tick;
    // esse delta já leva em consideração o tempo de viagem para a estacão em atraso
    auto const delta = millis() - starting_tick;

    LOG_IF(LogQueue, "perdeu um passo, compensando - [station = ", station.index(), " | starting_tick = ", starting_tick, " | delta =  ", delta, "ms]");

    recipe.map_remaining_steps(millis());
    execute_current_step(recipe, station);

    for_each_mapped_recipe(
        [delta](Recipe& other_recipe) {
            other_recipe.for_each_remaining_step([delta](Recipe::Step& step) {
                step.starting_tick += delta;
                return util::Iter::Continue;
            });
            return util::Iter::Continue;
        },
        &recipe);
}

// ao ocorrer uma mudança na fila (ex: cancelar uma recipe)
// as receitas que ainda não começaram são trazidas para frente, para evitar grandes períodos onde a máquina não faz nada
void RecipeQueue::remap_recipes_after_changes_in_queue() {
    util::StaticVector<size_t, Station::MAXIMUM_STATIONS> recipes_to_remap = {};

    for_each_mapped_recipe([&](Recipe& recipe, size_t index) {
        const size_t min_step = recipe.has_scalding_step() and recipe.scalded();
        const bool executed_a_step = recipe.current_step_index() > min_step;
        if (executed_a_step or m_recipe_in_execution == index)
            // caso a recipe ja tenha comecado não é remapeada
            return util::Iter::Continue;

        recipe.unmap_steps();
        recipes_to_remap.push_back(index);

        return util::Iter::Continue;
    });

    for (auto index : recipes_to_remap)
        map_recipe(m_queue[index].recipe, Station::list().at(index));
}

// esse algoritmo é basicamente um hit-test 2d de cada passo da recipe nova com cada passo das receitas já mapeadas
// é verificado se qualquer um dos passos da recipe nova colide com qualquer um dos passos de qualquer uma das receitas já mapeada
bool RecipeQueue::collides_with_other_recipes(Recipe const& new_recipe) const {
    bool colides = false;
    new_recipe.for_each_remaining_step([&](Recipe::Step const& new_step) {
        for_each_mapped_recipe(
            [&](const Recipe& mapped_recipe) {
                mapped_recipe.for_each_remaining_step([&](const Recipe::Step& mapped_step) {
                    // se o passo comeca antes do nosso comecar uma colisao é impossivel
                    if (mapped_step.starting_tick > (new_step.ending_tick() + util::TRAVEL_MARGIN)) {
                        // como todos os passos sao feitos em sequencia linear podemos parar de procurar nessa recipe
                        // ja que todos os passos subsequentes também serão depois desse
                        return util::Iter::Break;
                    }
                    colides = new_step.collides_with(mapped_step);
                    return colides ? util::Iter::Break : util::Iter::Continue;
                });
                return colides ? util::Iter::Break : util::Iter::Continue;
            },
            &new_recipe);
        return colides ? util::Iter::Break : util::Iter::Continue;
    });
    return colides;
}

void RecipeQueue::cancel_station_recipe(size_t index) {
    if (not m_queue[index].active) {
        LOG_ERR("tentando cancelar recipe de station que nao esta na fila - [station = ", index, "]");
        return;
    }

    remove_recipe(index);
    Station::list().at(index).set_status(Station::Status::Free);

    if (index == m_recipe_in_execution) {
        auto& recipe = m_queue[m_recipe_in_execution].recipe;
        if (not valid_timer(recipe.current_step().starting_tick)) {
            // o passo ainda não foi iniciado, ou seja, não estamos dentro da 'RecipeQueue::execute_current_step'
            recipe_was_cancelled(index);
        } else {
            // 'recipe_was_cancelled' vai ser chamada dentro da 'RecipeQueue::execute_current_step'
        }
        m_recipe_in_execution = Station::INVALID;
    } else {
        recipe_was_cancelled(index);
    }
}

void RecipeQueue::remove_finalized_recipes() {
    if (m_queue_size == 0) [[likely]]
        return;

    for_each_recipe([this](Recipe const& recipe, size_t index) {
        auto& station = Station::list().at(index);
        if (station.status() == Station::Status::Finalizing) [[unlikely]] {
            const auto delta = millis() - recipe.start_finalization_duration();
            if (delta >= recipe.finalization_duration()) {
                LOG_IF(LogQueue, "tempo de finalizacao acabou - [station = ", index, " | delta = ", delta, "ms]");
                station.set_status(Station::Status::Ready, recipe.id());
                remove_recipe(index);
            }
        }
        return util::Iter::Continue;
    });
}

// se a fila fica um certo periodo inativa o bico é enviado para o esgoto e despeja agua por alguns segundos
// com a finalidade de esquentar a mangueira e aliviar imprecisoes na hora de comecar um café
void RecipeQueue::try_heating_hose_after_inactivity() const {
    constexpr millis_t MAX_INACTIVITY_DURATION = 60000 * 10; // 10min
    static_assert(MAX_INACTIVITY_DURATION >= 60000, "minimo de 1 minuto");

    constexpr millis_t POUR_DURATION = 1000 * 20;                // 20s
    constexpr float POUR_VOLUME = 10.f * (POUR_DURATION / 1000); // 10 g/s

    if (s_tick_beginning_inactivity and millis() > s_tick_beginning_inactivity) {
        auto const delta = millis() - s_tick_beginning_inactivity;
        if (delta >= MAX_INACTIVITY_DURATION) {
            auto const delta_in_minutes = delta / 60000;
            LOG_IF(LogQueue, "aquecendo mangueira apos ", delta_in_minutes, "min de inatividade");

            Spout::the().travel_to_sewer();
            Spout::the().pour_with_desired_volume(POUR_DURATION, POUR_VOLUME, Spout::CorrectFlow::Yes);

            util::idle_while(
                [this] {
                    // se é recebido um pedido de recipe enquanto a maquina está no processo de aquecimento
                    // o numero de receitas em execucao aumenta, o bico é desligado e a recipe é executada
                    if (number_of_recipes_executing() != 0) {
                        Spout::the().end_pour();
                        return false;
                    }
                    return Spout::the().pouring();
                },
                Filters::RecipeQueue);

            LOG_IF(LogQueue, "aquecimento finalizado");
            s_tick_beginning_inactivity = millis();
        }
    }
}

void RecipeQueue::recipe_was_cancelled(size_t index) {
    LOG_IF(LogQueue, "recipe cancelada - [station = ", index, "]");
    remap_recipes_after_changes_in_queue();
}

// o conceito de uma recipe "em execucao" engloba somente os despejos em sí (escaldo e ataques)
// estacões que estão aguardando input do usuário ou finalizando não possuem seus passos pendentes mapeados
// consequentemente, não são consideradas como "em execucao" por mais que estejão na fila
size_t RecipeQueue::number_of_recipes_executing() const {
    size_t num = 0;
    for_each_mapped_recipe([&num](auto const&) {
        ++num;
        return util::Iter::Continue;
    });
    return num;
}

void RecipeQueue::send_queue_info(JsonArrayConst estacoes) const {
    info::event(
        "infoEstacoes",
        [&](JsonObject o) {
            if (m_recipe_in_execution != Station::INVALID)
                o["estacaoAtiva"] = m_recipe_in_execution;

            auto arr = o.createNestedArray("info");
            for (size_t i = 0; i < estacoes.size(); ++i) {
                const auto index = estacoes[i].as<size_t>();
                const auto& station = Station::list().at(index);
                if (station.blocked())
                    continue;

                auto obj = arr.createNestedObject();
                obj["estacao"] = station.index();
                obj["status"] = int(station.status());

                // os campos seguintes só aparecem para receitas em execução
                if (not m_queue[index].active)
                    continue;

                const auto tick = millis();
                const auto& recipe = m_queue[index].recipe;
                {
                    auto recipe_obj = obj.createNestedObject("infoReceita");
                    recipe_obj["receitaId"] = recipe.id();

                    const auto& first_step = recipe.first_step();
                    if (valid_timer(first_step.starting_tick, tick))
                        recipe_obj["tempoTotalReceita"] = tick - recipe.first_step().starting_tick;

                    const auto& first_attack = recipe.first_attack();
                    if (valid_timer(first_attack.starting_tick, tick))
                        recipe_obj["tempoTotalAtaques"] = tick - first_attack.starting_tick;

                    if (station.status() == Station::Status::Finalizing)
                        if (valid_timer(recipe.start_finalization_duration(), tick))
                            recipe_obj["progressoFinalizacao"] = tick - recipe.start_finalization_duration();
                }
                {
                    auto step_obj = obj.createNestedObject("infoPasso");
                    if (not recipe.current_step().starting_tick)
                        continue;

                    // se o passo ainda não comecou porém está mapeado ele pode estar ou no intervalo do ataque passado ou na fila para começar...
                    if (tick < recipe.current_step().starting_tick) {
                        // ...se é o escaldo ou o primeiro ataque não existe um "passo anterior", ou seja, não tem um intervalo - só pode estar na fila
                        // nesse caso nada é enviado para o app
                        if (recipe.current_step_index() <= size_t(recipe.has_scalding_step()))
                            continue;

                        // ...caso contrario, do segundo ataque pra frente, estamos no intervalo do passo anterior
                        const auto last_step_index = recipe.current_step_index() - 1;
                        const auto& last_step = recipe.step(last_step_index);
                        step_obj["index"] = last_step_index;
                        step_obj["progressoIntervalo"] = tick - last_step.ending_tick();
                    } else {
                        step_obj["index"] = recipe.current_step_index();
                        step_obj["progressoPasso"] = tick - recipe.current_step().starting_tick;
                    }
                }
            }
        });
}

void RecipeQueue::cancel_all_recipes() {
    for_each_recipe([this](Recipe& recipe, size_t index) {
        cancel_station_recipe(index);
        return util::Iter::Continue;
    });
}

void RecipeQueue::add_recipe(size_t index) {
    if (m_queue_size == m_queue.size()) {
        LOG_ERR("tentando adicionar recipe com a fila cheia - [station = ", index, "]");
        return;
    }

    m_queue_size++;
    m_queue[index].active = true;
}

void RecipeQueue::remove_recipe(size_t index) {
    if (m_queue_size == 0) {
        LOG_ERR("tentando remover recipe com a fila vazia - [station = ", index, "]");
        return;
    }

    m_queue_size--;
    m_queue[index].active = false;
}
}
