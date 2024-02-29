#include "RecipeQueue.h"
#include <lucas/Spout.h>
#include <lucas/Boiler.h>
#include <lucas/info/info.h>
#include <lucas/MotionController.h>
#include <lucas/core/core.h>
#include <lucas/util/ScopedGuard.h>
#include <lucas/util/StaticVector.h>

namespace lucas {
void RecipeQueue::setup() {
    m_storage_handle = storage::register_handle_for_entry("recipes", sizeof(m_fixed_recipes));
    if (auto entry = storage::fetch_entry(m_storage_handle))
        entry->read_binary_into(m_fixed_recipes);
}

void RecipeQueue::tick() {
    try_heating_hose_after_inactivity();

    if (m_recipe_in_execution != Station::INVALID) {
        if (not m_queue[m_recipe_in_execution].active) {
            LOG_ERR("estacao executando nao possui receita na fila - [estacao = ", m_recipe_in_execution, "]");
            m_recipe_in_execution = Station::INVALID;
            return;
        }

        auto& recipe = m_queue[m_recipe_in_execution].recipe;
        auto& station = Station::list().at(m_recipe_in_execution);
        const auto starting_tick = recipe.current_step().starting_tick;
        if (starting_tick == millis()) {
            execute_current_step(recipe, station);
        } else if (starting_tick < millis()) {
            compensate_for_missed_step(recipe, station);
        }
    } else {
        for_each_mapped_recipe([this](Recipe& recipe, usize index) {
            const auto starting_tick = recipe.current_step().starting_tick;
            // a ordem desses ifs importa por causa da subtracao de numeros unsigned, tome cuideido...
            if (starting_tick < millis()) {
                compensate_for_missed_step(recipe, Station::list().at(index));
                return util::Iter::Break;
            } else if (starting_tick - millis() <= util::TRAVEL_MARGIN) {
                // o passo está chegando...
                m_recipe_in_execution = index;
                LOG_IF(LogQueue, "passo esta prestes a comecar - [estacao = ", index, "]");
                MotionController::the().travel_to_station(index);
                return util::Iter::Break;
            }
            return util::Iter::Continue;
        });
    }
}

void RecipeQueue::schedule_recipe(JsonObjectConst recipe_json) {
    if (not recipe_json.containsKey("recipe") or
        not recipe_json.containsKey("station")) {
        LOG_ERR("json da receita nao possui todos os campos obrigatorios");
        return;
    }

    const auto station_index = recipe_json["station"].as<usize>();
    const auto recipe_obj = recipe_json["recipe"].as<JsonObjectConst>();
    auto& station = Station::list().at(station_index);
    if (station.status() != Station::Status::Free or station.blocked()) {
        LOG_ERR("tentando mapear receita de uma estacao invalida [estacao = ", station_index, "]");
        return;
    }

    auto& info = m_queue[station_index];
    info.recipe.build_from_json(recipe_obj);
    schedule_recipe_for_station(info.recipe, station_index);
}

void RecipeQueue::schedule_recipe_for_station(Recipe& recipe, usize index) {
    auto& station = Station::list().at(index);
    if (m_queue[index].active) {
        LOG_ERR("estacao ja possui uma receita na fila - [estacao = ", index, "]");
        return;
    }

    if (station.status() != Station::Status::Free or station.blocked()) {
        LOG_ERR("tentando agendar receita para uma station invalida [estacao = ", index, "]");
        return;
    }

    const auto status = recipe.has_scalding_step() ? Station::Status::ConfirmingScald : Station::Status::ConfirmingAttacks;
    station.set_status(status, recipe.id());
    add_recipe(index);
    LOG_IF(LogQueue, "receita agendada, aguardando confirmacao - [estacao = ", index, "]");
}

void RecipeQueue::set_fixed_recipes(JsonObjectConst recipe_json) {
    if (not recipe_json.containsKey("recipes")) {
        LOG_ERR("json da receita nao possui todos os campos obrigatorios");
        return;
    }

    auto recipes_array = recipe_json["recipes"].as<JsonArrayConst>();
    for (size_t i = 0; i < recipes_array.size(); i++) {
        auto recipe_obj = recipes_array[i];
        auto& info = m_fixed_recipes[i];
        if (not recipe_obj.isNull()) {
            info.recipe.build_from_json(recipe_obj);
            info.active = true;
            LOG_IF(LogQueue, "receita fixa setada - [estacao = ", i, "]");
        } else {
            info.recipe.reset();
            info.active = false;
            LOG_IF(LogQueue, "receita fixa removida - [estacao = ", i, "]");
        }
    }

    auto entry = storage::fetch_or_create_entry(m_storage_handle);
    entry.write_binary(m_fixed_recipes);
}

void RecipeQueue::reset_fixed_recipes() {
    storage::purge_entry(m_storage_handle);
    for (auto info : m_fixed_recipes) {
        info.recipe.reset();
        info.active = false;
    }
}

void RecipeQueue::map_station_recipe(usize index) {
    auto& station = Station::list().at(index);
    if (station.blocked())
        return;

    core::TemporaryFilter f{ core::Filter::Station };
    if (not m_queue[index].active) {
        if (m_fixed_recipes[index].active) {
            // don't execute fixed recipes when calibrating
            // the only recipe that should be executed during calibration is the cooling recipe
            if (core::calibration_phase() != core::CalibrationPhase::Done or
                not Boiler::the().is_in_coffee_making_temperature_range())
                return;

            auto& recipe = m_queue[index].recipe = m_fixed_recipes[index].recipe;

            add_recipe(index);
            map_recipe(recipe, station);

            station.set_status(recipe.has_scalding_step() ? Station::Status::Scalding : Station::Status::Attacking, recipe.id());
        } else {
            LOG_ERR("estacao nao possui receita na fila - [estacao = ", index, "]");
        }
    } else {
        // if the recipe is active then mapping means we must be waiting for some confirmation
        if (not station.waiting_user_confirmation()) {
            LOG_ERR("tentando mapear receita de uma station invalida [estacao = ", index, "]");
            return;
        }

        auto& recipe = m_queue[index].recipe;
        if (recipe.remaining_steps_are_mapped()) {
            LOG_ERR("receita ja esta mapeada - [estacao = ", index, "]");
            return;
        }

        // por conta do if acima nesse momento a recipe só pode estar em um de dois estados - 'ConfirmingScald' e 'ConfirmingAttacks'
        // os próximo estados após esses são 'Scalding' e 'Attacking', respectivamente
        const auto next_status = s32(station.status()) + 1;
        station.set_status(Station::Status(next_status), recipe.id());
        map_recipe(recipe, station);
    }
}

void RecipeQueue::map_recipe(Recipe& recipe, Station& station) {
    if (m_heating_hose_after_inactivity) {
        Spout::the().end_pour();
        m_heating_hose_after_inactivity = false;
    }

    util::StaticVector<millis_t, Station::MAXIMUM_NUMBER_OF_STATIONS> candidates;

    // tentamos encontrar um tick inicial que não causa colisões com nenhuma das outras recipe
    // esse processo é feito de forma bruta, homem das cavernas, mas como o número de receitas/passos é pequeno, não chega a ser um problema
    for_each_mapped_recipe(
        [&](const Recipe& mapped_recipe) {
            mapped_recipe.for_each_remaining_step([&](const Recipe::Step& step) {
                if (step.interval and step.interval < util::TRAVEL_MARGIN)
                    return util::Iter::Continue;

                // temos que sempre levar a margem de viagem em consideração quando procuramos pelo tick magico
                for (auto interval_offset = util::TRAVEL_MARGIN; interval_offset <= step.interval - util::TRAVEL_MARGIN; interval_offset += util::TRAVEL_MARGIN) {
                    const auto starting_tick = step.ending_tick() + interval_offset;
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
        first_step_tick = *candidates.min();
        recipe.map_remaining_steps(first_step_tick);
    } else {
        // se não foi achado nenhum candidato a fila está vazia
        // então a recipe é executada imediatamente
        MotionController::the().travel_to_station(station);
        first_step_tick = millis();
        m_recipe_in_execution = station.index();
        recipe.map_remaining_steps(first_step_tick);
    }
    LOG_IF(LogQueue, "receita mapeada - [estacao = ", station.index(), " | candidatos = ", candidates.size(), " | tick inicial = ", first_step_tick, "]");
}

// this mostly serves to avoid unsigned intenger underflow
// integener arithmetic is tricky!
static bool tick_has_happened(millis_t timer, millis_t tick = millis()) {
    return timer and tick >= timer;
};

void RecipeQueue::execute_current_step(Recipe& recipe, Station& station) {
    LOG_IF(LogQueue, "executando passo - [estacao = ", station.index(), " | passo = ", recipe.current_step_index(), " | tick = ", millis(), "]");

    const auto& current_step = recipe.current_step();
    const auto current_step_index = recipe.current_step_index();
    const auto dispatch_step_event = [](usize station, usize step, millis_t first_attack_tick, bool ending) {
        info::send(
            info::Event::Recipe,
            [&](JsonObject o) {
                o["station"] = station;
                o["step"] = step;
                o["done"] = ending;

                const auto tick = millis();
                if (tick_has_happened(first_attack_tick, tick))
                    o["timeElapsedAttacks"] = tick - first_attack_tick;
            });
    };

    {
        dispatch_step_event(station.index(), current_step_index, recipe.first_attack().starting_tick, false);

        m_recipe_in_execution = station.index();

        {
            core::TemporaryFilter f{ core::Filter::RecipeQueue }; // nada de tick()!
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

    const auto ideal_duration = current_step.duration;
    const auto actual_duration = millis() - current_step.starting_tick;
    const auto error = (ideal_duration > actual_duration) ? (ideal_duration - actual_duration) : (actual_duration - ideal_duration);
    LOG_IF(LogQueue, "passo acabou - [duracao = ", actual_duration, "ms | erro = ", error, "ms]");

    if (station.status() == Station::Status::Scalding) {
        station.set_status(Station::Status::ConfirmingAttacks, recipe.id());
    } else if (recipe.finished()) {
        if (recipe.finalization_duration() != 0ms) {
            station.set_status(Station::Status::Finalizing, recipe.id());
            recipe.finalization_timer().start();
        } else {
            station.set_status(Station::Status::Ready, recipe.id());
            remove_recipe(station.index());
        }
        LOG_IF(LogQueue, "receita acabou, finalizando - [estacao = ", station.index(), "]");
    }
}

// pode acontecer do passo de uma recipe nao ser executado no tick correto, pois a 'RecipeQueue::tick'
// nao é necessariamente executada a 1mhz, quando isso acontecer o passo atrasado é executado imediatamente
// e todos os outros sao compensados pelo tempo de atraso
void RecipeQueue::compensate_for_missed_step(Recipe& recipe, Station& station) {
    MotionController::the().travel_to_station(station);

    const auto starting_tick = recipe.current_step().starting_tick;
    // esse delta já leva em consideração o tempo de viagem para a estacão em atraso
    const auto delta = millis() - starting_tick;

    LOG_IF(LogQueue, "perdeu um passo, compensando - [estacao = ", station.index(), " | starting_tick = ", starting_tick, " | delta =  ", delta, "ms]");

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
    util::StaticVector<usize, Station::MAXIMUM_NUMBER_OF_STATIONS> recipes_to_remap = {};

    for_each_mapped_recipe([&](Recipe& recipe, usize index) {
        const usize min_step = recipe.has_scalding_step() and recipe.scalded();
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
bool RecipeQueue::collides_with_other_recipes(const Recipe& new_recipe) const {
    bool colides = false;
    new_recipe.for_each_remaining_step([&](const Recipe::Step& new_step, usize new_step_index) {
        for_each_mapped_recipe(
            [&](const Recipe& mapped_recipe, usize mapped_recipe_index) {
                mapped_recipe.for_each_remaining_step([&](const Recipe::Step& mapped_step, usize mapped_step_index) {
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

void RecipeQueue::cancel_station_recipe(usize index) {
    if (not m_queue[index].active) {
        LOG_ERR("tentando cancelar receita de estacao que nao esta na fila - [estacao = ", index, "]");
        return;
    }

    remove_recipe(index);
    Station::list().at(index).set_status(Station::Status::Free);

    if (index == m_recipe_in_execution) {
        MotionController::the().invalidate_location();
        auto& recipe = m_queue[m_recipe_in_execution].recipe;
        if (not tick_has_happened(recipe.current_step().starting_tick)) {
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

    for_each_recipe([this](const Recipe& recipe, usize index) {
        auto& station = Station::list().at(index);
        if (station.status() == Station::Status::Finalizing) [[unlikely]] {
            // FIXME: use a timer for this
            if (recipe.finalization_timer() >= recipe.finalization_duration()) {
                LOG_IF(LogQueue, "tempo de finalizacao acabou - [estacao = ", index, "]");
                station.set_status(Station::Status::Ready, recipe.id());
                remove_recipe(index);
            }
        }
        return util::Iter::Continue;
    });
}

// se a fila fica um certo periodo inativa o bico é enviado para o esgoto e despeja agua por alguns segundos
// com a finalidade de esquentar a mangueira e aliviar imprecisoes na hora de comecar um café
void RecipeQueue::try_heating_hose_after_inactivity() {
    if (core::calibration_phase() != core::CalibrationPhase::Done)
        return;

    enum class State {
        Waiting,
        Pouring
    };

    static State s_state = State::Waiting;

    constexpr auto POUR_DURATION = 1min;
    constexpr auto POUR_VOLUME = 10.f * chrono::duration_cast<chrono::seconds>(POUR_DURATION).count();
    constexpr auto INACTIVITY_THRESHOLD = 5min;

    switch (s_state) {
    case State::Waiting:
        if (m_inactivity_timer >= INACTIVITY_THRESHOLD) {
            info::send(
                info::Event::Other,
                [](JsonObject o) {
                    o["heatingHose"] = true;
                });

            core::TemporaryFilter f{ core::Filter::RecipeQueue, core::Filter::Station };
            m_heating_hose_after_inactivity = true;
            m_inactivity_timer.restart();

            LOG_IF(LogQueue, "aquecendo mangueira apos inatividade");

            MotionController::the().home();
            MotionController::the().travel_to_sewer();

            Spout::the().pour_with_desired_volume(POUR_DURATION, POUR_VOLUME);

            s_state = State::Pouring;
        }
        break;
    case State::Pouring:
        if (m_heating_hose_after_inactivity) {
            if (m_inactivity_timer >= POUR_DURATION) {
                LOG_IF(LogQueue, "mangueira aquecida");
                m_inactivity_timer.restart();
                s_state = State::Waiting;
            }
        } else {
            m_inactivity_timer.restart();
            s_state = State::Waiting;
            break;
        }
    }
}

void RecipeQueue::recipe_was_cancelled(usize index) {
    LOG_IF(LogQueue, "receita cancelada - [estacao = ", index, "]");
    remap_recipes_after_changes_in_queue();
}

// o conceito de uma recipe "em execucao" engloba somente os despejos em sí (escaldo e ataques)
// estacões que estão aguardando input do usuário ou finalizando não possuem seus passos pendentes mapeados
// consequentemente, não são consideradas como "em execucao" por mais que estejão na fila
usize RecipeQueue::number_of_recipes_being_executed() const {
    usize num = 0;
    for_each_mapped_recipe([&num](const auto&) {
        ++num;
        return util::Iter::Continue;
    });
    return num;
}

void RecipeQueue::send_queue_info(JsonArrayConst stations) const {
    info::send(
        info::Event::AllStations,
        [&](JsonObject o) {
            if (m_recipe_in_execution != Station::INVALID)
                o["activeStation"] = m_recipe_in_execution;

            auto arr = o.createNestedArray("stations");
            for (const auto json_variant : stations) {
                const auto index = json_variant.as<usize>();
                if (index >= Station::number_of_stations()) {
                    LOG_ERR("index invalido para requisicao - [index = ", index, "]");
                    continue;
                }

                const auto& station = Station::list().at(index);
                if (station.blocked())
                    continue;

                const auto& recipe = m_queue[index].recipe;
                const auto tick = millis();
                auto obj = arr.createNestedObject();
                obj["index"] = station.index();
                obj["status"] = s32(station.status());

                // os campos seguintes só aparecem para receitas em execução
                if (not m_queue[index].active)
                    continue;

                obj["recipeId"] = recipe.id();

                const auto& first_step = recipe.first_step();
                if (tick_has_happened(first_step.starting_tick, tick))
                    obj["timeElapsedTotal"] = tick - recipe.first_step().starting_tick;

                const auto& first_attack = recipe.first_attack();
                if (tick_has_happened(first_attack.starting_tick, tick))
                    obj["timeElapsedAttacks"] = tick - first_attack.starting_tick;

                if (station.status() == Station::Status::Finalizing)
                    if (recipe.finalization_timer().is_active())
                        obj["timeElapsedFinalization"] = recipe.finalization_timer().elapsed().count();

                if (not station.is_executing_or_in_queue())
                    continue;

                // se o passo ainda não comecou porém está mapeado ele pode estar ou no intervalo do ataque passado ou na fila para começar...
                if (tick < recipe.current_step().starting_tick) {
                    // ...sem um passo passo anterior não tem um intervalo - só pode estar na fila
                    // nesse caso nada é enviado para o app
                    if (not recipe.has_executed_first_attack())
                        continue;

                    // ...caso contrario, do segundo ataque pra frente, estamos no intervalo do passo anterior
                    const auto last_step_index = recipe.current_step_index() - 1;
                    const auto& last_step = recipe.step(last_step_index);
                    obj["step"] = last_step_index;
                    obj["timeElapsedInterval"] = tick - last_step.ending_tick();
                } else {
                    obj["step"] = recipe.current_step_index();
                    obj["timeElapsedStep"] = tick - recipe.current_step().starting_tick;
                }
            }
        });
}

void RecipeQueue::cancel_all_recipes() {
    for_each_recipe([this](Recipe& recipe, usize index) {
        cancel_station_recipe(index);
        return util::Iter::Continue;
    });
}

void RecipeQueue::reset_inactivity() {
    m_inactivity_timer.restart();
}

void RecipeQueue::add_recipe(usize index) {
    if (m_queue_size == Station::number_of_stations()) {
        LOG_ERR("tentando adicionar receita com a fila cheia - [estacao = ", index, "]");
        return;
    }

    m_queue_size++;
    m_queue[index].active = true;
}

void RecipeQueue::remove_recipe(usize index) {
    if (m_queue_size == 0) {
        LOG_ERR("tentando remover receita com a fila vazia - [estacao = ", index, "]");
        return;
    }

    m_queue_size--;
    m_queue[index].active = false;
}
}
