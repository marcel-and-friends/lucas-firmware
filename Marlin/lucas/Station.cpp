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

void Station::initialize(usize num) {
    if (num > MAXIMUM_NUMBER_OF_STATIONS) {
        LOG_ERR("numero de estacoes invalido - [max = ", MAXIMUM_NUMBER_OF_STATIONS, "]");
        return;
    }

    if (s_list_size != 0) {
        LOG_IF(LogStations, "estacoes ja foram inicializadas");
        return;
    }

    s_list_size = num;

    struct PinSetup {
        pin_t button_pin;
        pin_t led_pin;
        pin_t powerled_pin;
    };

    // PIN_WILSON
    constexpr auto PIN_DATA = std::array{
        PinSetup{.button_pin = PA1,  .led_pin = PD15, .powerled_pin = PD13},
        PinSetup{ .button_pin = PA3, .led_pin = PD8,  .powerled_pin = PE14},
        PinSetup{ .button_pin = PD3, .led_pin = PD9,  .powerled_pin = PC6 },
        PinSetup{ .button_pin = PB4, .led_pin = PB5,  .powerled_pin = PD11},
        PinSetup{ .button_pin = PD4, .led_pin = PB8,  .powerled_pin = PE13}
    };
    // constexpr auto PIN_DATA = std::array{
    //     PinSetup{.button_pin = PA1,  .led_pin = PD15, .powerled_pin = PE14},
    //     PinSetup{ .button_pin = PA3, .led_pin = PD8,  .powerled_pin = PE12},
    //     PinSetup{ .button_pin = PD3, .led_pin = PD9,  .powerled_pin = PE10},
    //     PinSetup{ .button_pin = PB4, .led_pin = PB5,  .powerled_pin = PE15},
    //     PinSetup{ .button_pin = PD4, .led_pin = PB8,  .powerled_pin = PC6 }
    // };

    for (usize i = 0; i < s_list_size; i++) {
        auto& pin_data = PIN_DATA.at(i);
        auto& station = s_list.at(i);

        station.set_button(pin_data.button_pin);
        station.set_led(pin_data.led_pin);
        station.set_powerled(pin_data.powerled_pin);

        station.set_status(Status::Free);
    }

    LOG_IF(LogStations, "maquina vai usar ", s_list_size, " estacoes");
}

void Station::tick() {
    for_each([](Station& station) {
        constexpr auto TIME_TO_CANCEL_RECIPE = 3s;

        const auto button_being_held = util::is_button_held(station.button());
        const auto button_clicked = not button_being_held and
                                    station.m_button_held_timer.is_active() and
                                    // check if the button has not just been released after canceling the recipe
                                    station.m_button_held_timer < TIME_TO_CANCEL_RECIPE;

        // allow one button press per second, this avoids accidental double presses caused by bad electronics
        if (button_clicked and station.m_last_button_press_timer >= 1s) {
            station.m_last_button_press_timer.restart();
            if (not CFG(MaintenanceMode)) {
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
            } else {
                LOG("BOTAO #", station.index() + 1, ": apertado");
            }
        }

        station.m_button_held_timer.toggle_based_on(button_being_held);
        if (not CFG(MaintenanceMode)) {
            if (station.m_button_held_timer >= TIME_TO_CANCEL_RECIPE and RecipeQueue::the().executing_recipe_in_station(station.index()))
                RecipeQueue::the().cancel_station_recipe(station.index());
        }

        return util::Iter::Continue;
    });
}

void Station::update_leds() {
    // é necessario manter um estado geral para que as leds pisquem juntas.
    // poderiamos simplificar essa funcao substituindo o 'WRITE(station.led(), ultimo_estado)' por 'TOGGLE(station.led())'
    // porém como cada estado dependeria do seu valor individual anterior as leds podem (e vão) sair de sincronia.
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
    const auto first_station_abs_pos = 86.f / MotionController::the().step_ratio_x();
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
    if (m_blocked == b)
        return;

    m_blocked = b;
    if (m_blocked)
        digitalWrite(m_led_pin, LOW);
    LOG_IF(LogStations, "estacao foi ", m_blocked ? "" : "des", "bloqueada - [index = ", AS_DIGIT(index()), "]");
}

void Station::set_status(Status status, std::optional<Recipe::Id> receita_id) {
    if (m_status == status)
        return;

    m_status = status;

    info::send(
        info::Event::Station,
        [this, &receita_id](JsonObject o) {
            o["station"] = index();
            o["status"] = s32(m_status);
            o["currentTemp"] = Boiler::the().temperature();
            if (receita_id)
                o["recipeId"] = *receita_id;
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
