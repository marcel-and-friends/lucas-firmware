#pragma once

#include <lucas/Station.h>
#include <lucas/Recipe.h>
#include <lucas/util/Timer.h>
#include <lucas/util/Singleton.h>
#include <ArduinoJson.h>
#include <unordered_map>
#include <vector>

namespace lucas {
class RecipeQueue : public util::Singleton<RecipeQueue> {
public:
    void setup();

    void tick();

    void schedule_recipe(JsonObjectConst recipe_json);

    void schedule_recipe_for_station(Recipe&, usize);

    void set_fixed_recipe(JsonObjectConst recipe_json);

    void map_station_recipe(usize);

    void cancel_station_recipe(usize);

    void remove_finalized_recipes();

    void send_queue_info(JsonArrayConst stations) const;

    void cancel_all_recipes();

    bool executing() const { return m_recipe_in_execution != Station::INVALID; }

    bool is_empty() const { return m_queue_size == 0; }

public:
    void for_each_recipe(util::IterFn<const Recipe&, usize> auto&& callback, const Recipe* exception = nullptr) const {
        if (m_queue_size == 0)
            return;

        for (usize i = 0; i < Station::number_of_stations(); ++i) {
            auto& info = m_queue[i];
            if (not info.active or (exception and exception == &info.recipe))
                continue;

            if (std::invoke(FWD(callback), info.recipe, i) == util::Iter::Break)
                return;
        }
    };

    void for_each_recipe(util::IterFn<Recipe&, usize> auto&& callback, const Recipe* exception = nullptr) {
        if (m_queue_size == 0)
            return;

        for (usize i = 0; i < Station::number_of_stations(); ++i) {
            auto& info = m_queue[i];
            if (not info.active or (exception and exception == &info.recipe))
                continue;

            if (std::invoke(FWD(callback), info.recipe, i) == util::Iter::Break)
                return;
        }
    };

    void for_each_mapped_recipe(util::IterFn<Recipe&> auto&& callback, const Recipe* exception = nullptr) {
        if (m_queue_size == 0)
            return;

        for (auto& info : m_queue) {
            if (not info.active or (exception and exception == &info.recipe) or not info.recipe.remaining_steps_are_mapped())
                continue;

            if (std::invoke(FWD(callback), info.recipe) == util::Iter::Break)
                return;
        }
    };

    void for_each_mapped_recipe(util::IterFn<const Recipe&> auto&& callback, const Recipe* exception = nullptr) const {
        if (m_queue_size == 0)
            return;

        for (auto& info : m_queue) {
            if (not info.active or (exception and exception == &info.recipe) or not info.recipe.remaining_steps_are_mapped())
                continue;

            if (std::invoke(FWD(callback), info.recipe) == util::Iter::Break)
                return;
        }
    };

    void for_each_mapped_recipe(util::IterFn<Recipe&, usize> auto&& callback, const Recipe* exception = nullptr) {
        if (m_queue_size == 0)
            return;

        for (usize i = 0; i < Station::number_of_stations(); ++i) {
            auto& info = m_queue[i];
            if (not info.active or (exception and exception == &info.recipe) or not info.recipe.remaining_steps_are_mapped())
                continue;

            if (std::invoke(FWD(callback), info.recipe, i) == util::Iter::Break)
                return;
        }
    };

    void for_each_mapped_recipe(util::IterFn<const Recipe&, usize> auto&& callback, const Recipe* exception = nullptr) const {
        if (m_queue_size == 0)
            return;

        for (usize i = 0; i < Station::number_of_stations(); ++i) {
            auto& info = m_queue[i];
            if (not info.active or (exception and exception == &info.recipe) or not info.recipe.remaining_steps_are_mapped())
                continue;

            if (std::invoke(FWD(callback), info.recipe, i) == util::Iter::Break)
                return;
        }
    };

private:
    void execute_current_step(Recipe&, Station&);

    void map_recipe(Recipe&, Station&);

    void compensate_for_missed_step(Recipe&, Station&);

    void remap_recipes_after_changes_in_queue();

    void try_heating_hose_after_inactivity();

    bool collides_with_other_recipes(const Recipe&) const;

    void add_recipe(usize);

    void remove_recipe(usize);

    void recipe_was_cancelled(usize index);

    usize number_of_recipes_executing() const;

private:
    util::Timer m_inactivity_timer;
    usize m_recipe_in_execution = Station::INVALID;

    struct RecipeInfo {
        Recipe recipe;
        bool active = false;
    };

    // o mapeamento de index -> recipe é o mesmo de index -> estação
    // ou seja, a recipe na posição 0 da fila pertence à estação 0
    std::array<RecipeInfo, Station::MAXIMUM_NUMBER_OF_STATIONS> m_queue = {};
    std::array<RecipeInfo, Station::MAXIMUM_NUMBER_OF_STATIONS> m_fixed_recipes = {};
    usize m_queue_size = 0;
};
}
