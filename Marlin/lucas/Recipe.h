#pragma once

#include <cstddef>
#include <concepts>
#include <cstdint>
#include <string_view>
#include <array>
#include <memory>
#include <span>
#include <src/MarlinCore.h>
#include <lucas/util/util.h>
#include <ArduinoJson.h>

namespace lucas {
class Recipe {
public:
    struct Step {
        // o tick em que esse passo será executado
        // @RecipeQueue::map_recipe
        millis_t starting_tick = 0;

        // a duracao total
        millis_t duration = 0;

        // o tempo de intervalo entre esse e o proximo passo
        millis_t interval = 0;

        // o gcode desse passo
        char gcode[48] = {};

        millis_t ending_tick() const {
            if (starting_tick == 0)
                return 0;
            return starting_tick + duration;
        }

        bool collides_with(Step const& other) const;
    };

public:
    static JsonObjectConst standard();

    void build_from_json(JsonObjectConst);

    Recipe(Recipe const&) = delete;
    Recipe(Recipe&&) = delete;
    Recipe& operator=(Recipe const&) = delete;
    Recipe& operator=(Recipe&&) = delete;

public:
    void execute_current_step();

    void map_remaining_steps(millis_t tick_inicial);

    void unmap_steps();

    bool remaining_steps_are_mapped() const;

public:
    void for_each_remaining_step(util::IterFn<Step&> auto&& callback) {
        if (m_has_scalding_step and not scalded()) {
            std::invoke(FWD(callback), m_steps.front());
            return;
        }

        for (size_t i = m_current_step; i < m_steps_size; ++i)
            if (std::invoke(FWD(callback), m_steps[i]) == util::Iter::Break)
                return;
    }

    void for_each_remaining_step(util::IterFn<Step&, size_t> auto&& callback) {
        if (m_has_scalding_step and not scalded()) {
            std::invoke(FWD(callback), m_steps.front());
            return;
        }

        for (size_t i = m_current_step; i < m_steps_size; ++i)
            if (std::invoke(FWD(callback), m_steps[i], i) == util::Iter::Break)
                return;
    }

    void for_each_remaining_step(util::IterFn<Step const&> auto&& callback) const {
        if (m_has_scalding_step and not scalded()) {
            std::invoke(FWD(callback), m_steps.front());
            return;
        }

        for (size_t i = m_current_step; i < m_steps_size; ++i)
            if (std::invoke(FWD(callback), m_steps[i]) == util::Iter::Break)
                return;
    }

    void for_each_remaining_step(util::IterFn<Step const&, size_t> auto&& callback) const {
        if (m_has_scalding_step and not scalded()) {
            std::invoke(FWD(callback), m_steps.front());
            return;
        }

        for (size_t i = m_current_step; i < m_steps_size; ++i)
            if (std::invoke(FWD(callback), m_steps[i], i) == util::Iter::Break)
                return;
    }

public:
    bool has_scalding_step() const { return m_has_scalding_step; }
    Step const& scalding_step() const { return m_steps.front(); }
    Step& scalding_step() { return m_steps.front(); }

    millis_t finalization_duration() const { return m_finalization_duration; }

    millis_t start_finalization_duration() const { return m_start_finalization_duration; }
    void set_start_finalization_duration(millis_t tick) { m_start_finalization_duration = tick; }

    uint32_t id() const { return m_id; }

    bool scalded() const { return m_has_scalding_step and m_current_step > 0; }

    Step const& first_attack() const { return m_steps[m_has_scalding_step]; }

    Step const& current_step() const { return m_steps[m_current_step]; }
    Step& current_step() { return m_steps[m_current_step]; }

    size_t current_step_index() const { return m_current_step; }

    bool finished() const { return m_current_step == m_steps_size; }

    Step const& first_step() const { return m_steps.front(); }

    Step const& step(size_t index) const { return m_steps[index]; }

private:
    void reset();

    // as receitas, similarmente às estacões, existem exclusivemente como elementos de um array estático, na classe RecipeQueue
    // por isso os copy/move constructors são deletados e só a RecipeQueue pode acessar o constructor default
    friend class RecipeQueue;
    Recipe() = default;

    uint32_t m_id = 0;

    millis_t m_finalization_duration = 0;
    millis_t m_start_finalization_duration = 0;

    bool m_has_scalding_step = false;

    static constexpr auto MAX_ATTACKS = 10;            // foi decidido por marcel
    static constexpr auto MAX_STEPS = MAX_ATTACKS + 1; // incluindo o escaldo
    // finge que isso é um vector
    std::array<Step, MAX_STEPS> m_steps = {};
    size_t m_steps_size = 0;

    // a index dos passos de uma recipe depende da existencia do escaldo
    // o primeiro ataque de uma recipe com escaldo tem index 1
    // o primeiro ataque de uma recipe sem escaldo tem index 0
    size_t m_current_step = 0;
};
}
