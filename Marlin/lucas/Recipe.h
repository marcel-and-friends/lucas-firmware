#pragma once

#include <src/MarlinCore.h>
#include <lucas/util/Timer.h>
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

        bool collides_with(const Step& other) const;
    };

public:
    static JsonObjectConst standard();

    void build_from_json(JsonObjectConst);

    Recipe(const Recipe&) = delete;
    Recipe(Recipe&&) = delete;
    Recipe& operator=(const Recipe&) = delete;
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

    void for_each_remaining_step(util::IterFn<const Step&> auto&& callback) const {
        if (m_has_scalding_step and not scalded()) {
            std::invoke(FWD(callback), m_steps.front());
            return;
        }

        for (size_t i = m_current_step; i < m_steps_size; ++i)
            if (std::invoke(FWD(callback), m_steps[i]) == util::Iter::Break)
                return;
    }

    void for_each_remaining_step(util::IterFn<const Step&, size_t> auto&& callback) const {
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
    const Step& scalding_step() const { return m_steps.front(); }
    Step& scalding_step() { return m_steps.front(); }

    chrono::milliseconds finalization_duration() const { return m_finalization_duration; }

    const util::Timer& finalization_timer() const { return m_finalization_timer; }
    util::Timer& finalization_timer() { return m_finalization_timer; }

    uint32_t id() const { return m_id; }

    bool scalded() const { return m_has_scalding_step and m_current_step > 0; }

    const Step& first_attack() const { return m_steps[m_has_scalding_step]; }

    const Step& current_step() const { return m_steps[m_current_step]; }
    Step& current_step() { return m_steps[m_current_step]; }

    size_t current_step_index() const { return m_current_step; }

    bool finished() const { return m_current_step == m_steps_size; }

    bool has_executed_first_attack() const { return m_current_step > m_has_scalding_step; }

    const Step& first_step() const { return m_steps.front(); }

    const Step& step(size_t index) const { return m_steps[index]; }

private:
    void reset();

    // as receitas, similarmente às estacões, existem exclusivemente como elementos de um array estático, na classe RecipeQueue
    // por isso os copy/move constructors são deletados e só a RecipeQueue pode acessar o constructor default
    friend class RecipeQueue;
    Recipe() = default;

    uint32_t m_id = 0;

    chrono::milliseconds m_finalization_duration = {};
    util::Timer m_finalization_timer;

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
