#pragma once

#include <string>
#include <optional>
#include <lucas/lucas.h>
#include <lucas/Recipe.h>
#include <lucas/util/Timer.h>
#include <ArduinoJson.h>

namespace lucas {
class Station {
public:
    static constexpr usize INVALID = static_cast<usize>(-1);

    static constexpr usize MAXIMUM_NUMBER_OF_STATIONS = 5;
    using List = std::array<Station, MAXIMUM_NUMBER_OF_STATIONS>;

    static void initialize(usize num);

    static void for_each(util::IterFn<Station&> auto&& callback) {
        if (s_list_size == 0)
            return;

        for (usize i = 0; i < s_list_size; ++i) {
            auto& station = s_list[i];
            if (station.m_blocked)
                continue;
            if (std::invoke(FWD(callback), station) == util::Iter::Break)
                break;
        }
    }

    static void for_each(util::IterFn<Station&, usize> auto&& callback) {
        if (s_list_size == 0)
            return;

        for (usize i = 0; i < s_list_size; ++i) {
            auto& station = s_list[i];
            if (station.m_blocked)
                continue;
            if (std::invoke(FWD(callback), station, i) == util::Iter::Break)
                break;
        }
    }

    static void for_each_if(util::Fn<bool, Station&> auto&& condition, util::IterFn<Station&> auto&& callback) {
        if (s_list_size == 0)
            return;

        for (usize i = 0; i < s_list_size; ++i) {
            auto& station = s_list[i];
            if (station.m_blocked)
                continue;
            if (std::invoke(FWD(condition), station)) {
                if (std::invoke(FWD(callback), station) == util::Iter::Break)
                    break;
            }
        }
    }

    static void tick();

    static void update_leds();

    static List& list() { return s_list; }

    static float absolute_position(usize index);

    static usize number_of_stations() { return s_list_size; }

    Station(const Station&) = delete;
    Station(Station&&) = delete;
    Station& operator=(const Station&) = delete;
    Station& operator=(Station&&) = delete;

public:
    // muito importante manter esse enum em sincronia com o app
    // FREE = 0,
    // WAITING_START,
    // SCALDING,
    // INITIALIZE_COFFEE,
    // MAKING_COFFEE,
    // NOTIFICATION_TIME,
    // IS_READY

    enum class Status {
        Free = 0,
        ConfirmingScald,
        Scalding,
        ConfirmingAttacks,
        Attacking,
        Finalizing,
        Ready
    };

    usize number() const;
    usize index() const;

    bool waiting_user_confirmation() const { return m_status == Status::ConfirmingScald or m_status == Status::ConfirmingAttacks; }
    bool waiting_user_input() const { return waiting_user_confirmation() or m_status == Status::Ready; }
    bool is_executing_or_in_queue() const { return m_status == Status::Scalding or m_status == Status::Attacking; }

public:
    pin_t button() const { return m_button_pin; }
    pin_t led() const { return m_led_pin; }
    pin_t powerled() const { return m_powerled_pin; }

    bool recipe_was_cancelled() const { return m_recipe_was_cancelled; }
    void set_recipe_was_cancelled(bool b) { m_recipe_was_cancelled = b; }

    bool blocked() const { return m_blocked; }
    void set_blocked(bool);

    Status status() const { return m_status; }
    void set_status(Status, std::optional<u32> receita_id = std::nullopt);

private:
    // initialized out of line porque nesse momento a classe 'Station' é incompleta
    static List s_list;

    static inline usize s_list_size = 0;

    void set_button(pin_t pin);
    void set_led(pin_t pin);
    void set_powerled(pin_t pin);

private:
    Station() = default;

    // status, usado pelo app
    Status m_status = Status::Free;

    // o pin físico do nosso botão
    pin_t m_button_pin = -1;

    // o pin físico da nossa led
    pin_t m_led_pin = -1;

    // o pin físico da nossa powerled
    pin_t m_powerled_pin = -1;

    // usado para cancelar uma recipe
    util::Timer m_button_held_timer = {};

    // se a recipe acabou de ser cancelada e o button ainda nao foi solto
    bool m_recipe_was_cancelled = false;

    // no app voce tem a possibilidade de bloquear um boca
    bool m_blocked = false;
};
}
