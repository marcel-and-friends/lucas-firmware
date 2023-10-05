#include "Station.h"
#include <memory>
#include <src/module/temperature.h>
#include <lucas/info/info.h>
#include <lucas/Spout.h>
#include <lucas/Boiler.h>
#include <lucas/RecipeQueue.h>
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

    struct InfoStation {
        pin_t button_pin;
        pin_t led_pin;
        pin_t powerled_pin;
    };

    constexpr std::array<InfoStation, Station::MAXIMUM_NUMBER_OF_STATIONS> infos = {
        InfoStation{.button_pin = PA1,  .led_pin = PD15, .powerled_pin = Z_STEP_PIN },
        InfoStation{ .button_pin = PA3, .led_pin = PD8,  .powerled_pin = E0_STEP_PIN},
        InfoStation{ .button_pin = PD3, .led_pin = PD9,  .powerled_pin = E1_STEP_PIN},
        InfoStation{ .button_pin = PB4, .led_pin = PB5,  .powerled_pin = Z_DIR_PIN  },
        InfoStation{ .button_pin = PD4, .led_pin = PB8,  .powerled_pin = E0_DIR_PIN }
    };

    for (usize i = 0; i < num; i++) {
        auto& info = infos.at(i);
        auto& station = s_list.at(i);
        station.set_button(info.button_pin);
        station.set_led(info.led_pin);
        station.set_status(Status::Free);
    }

    s_list_size = num;

    LOG_IF(LogStations, "maquina vai usar ", num, " estacoes");
}

void Station::tick() {
    for_each([](Station& station) {
        const auto tick = millis();

        // atualizacao simples do estado do button
        bool held_before = station.is_button_held();
        bool held_now = util::is_button_held(station.button());
        station.set_is_button_held(held_now);

        // {
        //     constexpr auto TIME_TO_CANCEL_RECIPE = 3000; // 3s
        //     if (held_now) {
        //         if (not station.tick_button_was_pressed())
        //             station.set_tick_button_was_pressed(tick);

        //         if (tick - station.tick_button_was_pressed() >= TIME_TO_CANCEL_RECIPE) {
        //             RecipeQueue::the().cancel_station_recipe(station.index());
        //             station.set_recipe_was_cancelled(true);
        //             return util::Iter::Continue;
        //         }
        //     }
        // }

        if (held_before and not held_now) {
            LOG("botao da estacao ", station.number(), " foi solto");
            station.set_tick_button_was_pressed(0);
        }

        if (station.recipe_was_cancelled()) {
            // se a recipe acabou de ser cancelada, podemos voltar ao normal
            // o proposito disso é não enviar a recipe standard imediatamente após cancelar uma recipe
            station.set_recipe_was_cancelled(false);
            return util::Iter::Continue;
        }

        switch (station.status()) {
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
            WRITE(station.led(), s_last_led_state);
            return util::Iter::Continue;
        });
    }
}

float Station::absolute_position(usize index) {
    return util::first_station_abs_pos() + index * util::distance_between_each_station();
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
    analogWrite(m_led_pin, LOW);
}

void Station::set_powerled(pin_t pin) {
    if (m_powerled_pin == pin)
        return;

    m_powerled_pin = pin;
    SET_OUTPUT(m_powerled_pin);
    analogWrite(m_powerled_pin, LOW);
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
        WRITE(m_led_pin, LOW);
    LOG_IF(LogStations, "estacao foi ", m_blocked ? "" : "des", "bloqueada - [index = ", AS_DIGIT(index()), "]");
}

void Station::set_status(Status status, std::optional<u32> receita_id) {
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
        WRITE(m_led_pin, LOW);
        return;
    case Status::Scalding:
    case Status::Attacking:
    case Status::Finalizing:
        WRITE(m_led_pin, HIGH);
        return;
    default:
        return;
    }
}
}
