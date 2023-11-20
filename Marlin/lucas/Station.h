#pragma once

#include <lucas/lucas.h>
#include <lucas/Recipe.h>
#include <lucas/util/Timer.h>
#include <lucas/storage/storage.h>
#include <optional>

namespace lucas {
class Station {
public:
    static constexpr usize INVALID = static_cast<usize>(-1);

    static constexpr usize MAXIMUM_NUMBER_OF_STATIONS = 5;

    template<typename T>
    using SharedData = std::array<T, MAXIMUM_NUMBER_OF_STATIONS>;

    using List = SharedData<Station>;

    static void setup();

    static void setup_pins(usize num);

    static void initialize(std::optional<usize> num, std::optional<SharedData<bool>> blocked_stations);

    static void for_each(util::IterFn<Station&> auto&& callback) {
        if (s_list_size == 0)
            return;

        for (usize i = 0; i < s_list_size; ++i) {
            auto& station = s_list[i];
            if (station.blocked())
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
            if (station.blocked())
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
            if (station.blocked())
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

    bool blocked() const { return s_blocked_stations[index()]; }
    void set_blocked(bool);

    Status status() const { return m_status; }
    void set_status(Status, std::optional<Recipe::Id> receita_id = std::nullopt);

private:
    // initialized out of line porque nesse momento a classe 'Station' é incompleta
    static List s_list;
    static inline usize s_list_size = 0;
    static inline storage::Handle s_list_size_storage_handle;

    static inline SharedData<bool> s_blocked_stations = {};
    static inline storage::Handle s_blocked_stations_storage_handle;

    void set_button(pin_t pin);
    void set_led(pin_t pin);
    void set_powerled(pin_t pin);

    static void update_blocked_stations_storage();

private:
    Station() = default;

    Status m_status = Status::Free;

    pin_t m_button_pin = -1;

    pin_t m_led_pin = -1;

    pin_t m_powerled_pin = -1;

    util::Timer m_button_held_timer = {};

    // this begins as 'started' so that the first button press is not ignored
    util::Timer m_last_button_press_timer = util::Timer::started();
};
}
