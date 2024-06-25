#include "Station.h"
#include <memory>
#include <src/module/temperature.h>
#include <lucas/info/info.h>
#include <lucas/Spout.h>
#include <lucas/Boiler.h>
#include <lucas/RecipeQueue.h>
#include <lucas/MotionController.h>
#include <lucas/cmd/cmd.h>

namespace lucas {
Station::List Station::s_list = {};

void Station::setup() {
    s_list_size_storage_handle = storage::register_handle_for_entry("stations", sizeof(s_list_size));
    s_blocked_stations_storage_handle = storage::register_handle_for_entry("blocked", sizeof(s_blocked_stations));
}

void Station::initialize(std::optional<usize> num, std::optional<SharedData<bool>> blocked_stations) {
    if (num > MAXIMUM_NUMBER_OF_STATIONS) {
        LOG_ERR("numero de estacoes invalido - [max = ", MAXIMUM_NUMBER_OF_STATIONS, "]");
        return;
    }

    if (s_list_size == 0) {
        s_list_size = storage::create_or_update_entry(s_list_size_storage_handle, num, 3uz);
        s_blocked_stations = storage::create_or_update_entry(s_blocked_stations_storage_handle, blocked_stations, { false, false, false, false, false });
        setup_pins(s_list_size);

        for_each([](Station& station) {
            station.set_status(Status::Free);
            return util::Iter::Continue;
        });

        LOG_IF(LogStations, "maquina vai usar ", s_list_size, " estacoes");
    } else {
        if (blocked_stations) {
            s_blocked_stations = *blocked_stations;
            update_blocked_stations_storage();
        }
    }
}

struct PinData {
    pin_t button_pin;
    pin_t led_pin;
    pin_t powerled_pin;
};

void Station::setup_pins(usize number_of_stations) {
    constexpr auto PIN_DATA = std::array{
        PinData{ .button_pin = PA1, .led_pin = PD15, .powerled_pin = PD13 },
        PinData{ .button_pin = PA3, .led_pin = PD8,  .powerled_pin = PE14 },
        PinData{ .button_pin = PD3, .led_pin = PD9,  .powerled_pin = PC6  },
        PinData{ .button_pin = PB4, .led_pin = PB5,  .powerled_pin = PD11 },
        PinData{ .button_pin = PD4, .led_pin = PB8,  .powerled_pin = PE13 }
    };

    for (usize i = 0; i < number_of_stations; i++) {
        auto& pin_data = PIN_DATA.at(i);
        auto& station = s_list.at(i);

        station.set_button(pin_data.button_pin);
        station.set_led(pin_data.led_pin);
        station.set_powerled(pin_data.powerled_pin);
    }
}

void Station::tick() {
    if (CFG(MaintenanceMode)) {
        // this can't be inlined on the for loop because it crashes the cpu. yes. crashes. the cpu. :)
        size_t max = s_list_size ?: MAXIMUM_NUMBER_OF_STATIONS;
        for (size_t i = 0; i < max; ++i) {
            auto& station = s_list[i];
            // this should never happen but better be safe than sorry
            if (station.button() == -1)
                continue;

            const auto button_being_held = util::is_button_held(station.button());
            const auto button_clicked = not button_being_held and
                                        station.m_button_held_timer.is_active();

            station.m_button_held_timer.toggle_based_on(button_being_held);

            if (button_clicked) {
                LOG("BOTAO #", station.index() + 1, ": apertado");
            }
        }
    } else {
        for_each([](Station& station) {
            constexpr auto TIME_TO_CANCEL_RECIPE = 3s;
            constexpr auto BUTTON_DEBOUNCE = 200ms;

            const auto button_being_held = util::is_button_held(station.button());
            const auto button_clicked = not button_being_held and
                                        station.m_button_held_timer.is_active() and
                                        // check if the button has not just been released after canceling the recipe
                                        station.m_button_held_timer < TIME_TO_CANCEL_RECIPE;

            if (button_clicked and station.m_last_button_press_timer >= BUTTON_DEBOUNCE) {
                station.m_last_button_press_timer.restart();
                switch (station.status()) {
                case Status::Free:
                case Status::ConfirmingScald:
                case Status::ConfirmingAttacks:
                    RecipeQueue::the().map_station_recipe(station.index());
                    break;
                case Status::Ready:
                    station.set_status(Status::Free);
                    break;
                default:
                    break;
                }
            }

            station.m_button_held_timer.toggle_based_on(button_being_held);
            if (station.m_button_held_timer >= TIME_TO_CANCEL_RECIPE and RecipeQueue::the().is_executing_recipe_in_station(station.index()))
                RecipeQueue::the().cancel_station_recipe(station.index());

            return util::Iter::Continue;
        });
    }
}

void Station::update_leds() {
    // its necessary to maintain a general state so that the LEDs blink together.
    // we could simplify this function by replacing 'WRITE(station.led(), last_state)' with 'TOGGLE(station.led())',
    // but since each state depends on its individual previous value, the LEDs can (and will) go out of sync.
    every(500ms) {
        static bool s_last_led_state = true;
        s_last_led_state = not s_last_led_state;
        for_each_if(&Station::waiting_user_input, [](const Station& station) {
            digitalWrite(station.led(), s_last_led_state);
            return util::Iter::Continue;
        });
    }
}

float Station::absolute_position(usize index) {
    const auto first_station_abs_pos = 85.f / MotionController::the().step_ratio_x();
    const auto distance_between_each_station = 160.f / MotionController::the().step_ratio_x();
    return first_station_abs_pos + index * distance_between_each_station;
}

usize Station::number() const {
    return index() + 1;
}

usize Station::index() const {
    // cute
    return ((uintptr_t)this - (uintptr_t)&s_list) / sizeof(Station);
}

void Station::set_led(pin_t pin) {
    if (m_led_pin == pin)
        return;

    m_led_pin = pin;
    SET_OUTPUT(m_led_pin);
    digitalWrite(m_led_pin, LOW);
}

void Station::set_powerled(pin_t pin) {
    if (m_powerled_pin == pin)
        return;

    m_powerled_pin = pin;
    SET_OUTPUT(m_powerled_pin);
    digitalWrite(m_powerled_pin, HIGH);
}

void Station::set_button(pin_t pin) {
    if (m_button_pin == pin)
        return;

    m_button_pin = pin;
    SET_INPUT_PULLUP(m_button_pin);
}

void Station::set_blocked(bool b) {
    auto& blocked = s_blocked_stations[index()];
    if (blocked == b)
        return;

    blocked = b;
    if (blocked)
        digitalWrite(m_led_pin, LOW);

    update_blocked_stations_storage();

    LOG_IF(LogStations, "estacao foi ", blocked ? "" : "des", "bloqueada - [index = ", AS_DIGIT(index()), "]");
}

void Station::update_blocked_stations_storage() {
    auto entry = storage::fetch_or_create_entry(s_blocked_stations_storage_handle);
    entry.write_binary(s_blocked_stations);
}

void Station::set_status(Status status, std::optional<Recipe::Id> recipe_id) {
    if (m_status == status)
        return;

    m_status = status;

    info::send(
        info::Event::Station,
        [this, &recipe_id](JsonObject o) {
            o["station"] = index();
            o["status"] = s32(m_status);
            o["currentTemp"] = Boiler::the().temperature();
            if (recipe_id)
                o["recipeId"] = *recipe_id;
        });

    switch (m_status) {
    case Status::Free:
        digitalWrite(m_led_pin, LOW);
        digitalWrite(m_powerled_pin, HIGH);
        return;
    case Status::ConfirmingScald:
    case Status::Scalding:
    case Status::ConfirmingAttacks:
    case Status::Attacking:
    case Status::Finalizing:
    case Status::Ready:
        digitalWrite(m_led_pin, HIGH);
        digitalWrite(m_powerled_pin, LOW);
        return;
    }
}
}
