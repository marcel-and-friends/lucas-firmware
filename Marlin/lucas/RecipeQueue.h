#pragma once

#include <lucas/Station.h>
#include <lucas/Recipe.h>
#include <ArduinoJson.h>
#include <unordered_map>
#include <vector>

namespace lucas {
class RecipeQueue {
public:
    static RecipeQueue& the() {
        static RecipeQueue instance;
        return instance;
    }

    void tick();

    void schedule_recipe(JsonObjectConst recipe_json);

    void schedule_recipe_for_station(Recipe&, size_t);

    void map_station_recipe(size_t);

    void cancel_station_recipe(size_t);

    void remove_finalized_recipes();

    void try_heating_hose_after_inactivity() const;

    void send_queue_info(JsonArrayConst stations) const;

    bool executing() const { return m_recipe_in_execution != Station::INVALID; }

public:
    void for_each_recipe(util::IterFn<Recipe const&, size_t> auto&& callback, Recipe const* exception = nullptr) const {
        if (m_queue_size == 0) [[likely]]
            return;

        for (size_t i = 0; i < m_queue.size(); ++i) {
            auto& info = m_queue[i];
            if (not info.active or (exception and exception == &info.recipe))
                continue;

            if (std::invoke(FWD(callback), info.recipe, i) == util::Iter::Break)
                return;
        }
    };

    void for_each_mapped_recipe(util::IterFn<Recipe&> auto&& callback, Recipe const* exception = nullptr) {
        if (m_queue_size == 0) [[likely]]
            return;

        for (auto& info : m_queue) {
            if (not info.active or (exception and exception == &info.recipe) or not info.recipe.remaining_steps_are_mapped())
                continue;

            if (std::invoke(FWD(callback), info.recipe) == util::Iter::Break)
                return;
        }
    };

    void for_each_mapped_recipe(util::IterFn<Recipe const&> auto&& callback, Recipe const* exception = nullptr) const {
        if (m_queue_size == 0) [[likely]]
            return;

        for (auto& info : m_queue) {
            if (not info.active or (exception and exception == &info.recipe) or not info.recipe.remaining_steps_are_mapped())
                continue;

            if (std::invoke(FWD(callback), info.recipe) == util::Iter::Break)
                return;
        }
    };

    void for_each_mapped_recipe(util::IterFn<Recipe&, size_t> auto&& callback, Recipe const* exception = nullptr) {
        if (m_queue_size == 0) [[likely]]
            return;

        for (size_t i = 0; i < m_queue.size(); ++i) {
            auto& info = m_queue[i];
            if (not info.active or (exception and exception == &info.recipe) or not info.recipe.remaining_steps_are_mapped())
                continue;

            if (std::invoke(FWD(callback), info.recipe, i) == util::Iter::Break)
                return;
        }
    };

    void for_each_mapped_recipe(util::IterFn<Recipe const&, size_t> auto&& callback, Recipe const* exception = nullptr) const {
        if (m_queue_size == 0) [[likely]]
            return;

        for (size_t i = 0; i < m_queue.size(); ++i) {
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

    bool collides_with_other_recipes(Recipe const&) const;

    void add_recipe(size_t);

    void remove_recipe(size_t);

    void recipe_was_cancelled(size_t index);

    size_t number_of_recipes_executing() const;

private:
    size_t m_recipe_in_execution = Station::INVALID;

    struct RecipeInfo {
        Recipe recipe;
        bool active = false;
    };

    // o mapeamento de index -> recipe é o mesmo de index -> estação
    // ou seja, a recipe na posição 0 da fila pertence à estação 0
    std::array<RecipeInfo, Station::MAXIMUM_STATIONS> m_queue = {};
    size_t m_queue_size = 0;
};
}
