#pragma once

#include <array>
#include <string>
#include <optional>
#include <string_view>
#include <lucas/lucas.h>
#include <lucas/Recipe.h>
#include <ArduinoJson.h>

namespace lucas {
class Station {
public:
    static constexpr size_t INVALID = static_cast<size_t>(-1);

    static constexpr size_t MAXIMUM_STATIONS = 5;
    using List = std::array<Station, MAXIMUM_STATIONS>;

    static void initialize(size_t num);

    static void for_each(util::IterFn<Station&> auto&& callback) {
        if (s_list_size == 0)
            return;

        for (size_t i = 0; i < s_list_size; ++i)
            if (std::invoke(FWD(callback), s_list[i]) == util::Iter::Break)
                break;
    }

    static void for_each(util::IterFn<Station&, size_t> auto&& callback) {
        if (s_list_size == 0)
            return;

        for (size_t i = 0; i < s_list_size; ++i)
            if (std::invoke(FWD(callback), s_list[i], i) == util::Iter::Break)
                break;
    }

    static void for_each_if(util::Fn<bool, Station&> auto&& condition, util::IterFn<Station&> auto&& callback) {
        if (s_list_size == 0)
            return;

        for (size_t i = 0; i < s_list_size; ++i) {
            if (std::invoke(FWD(condition), s_list[i])) {
                if (std::invoke(FWD(callback), s_list[i]) == util::Iter::Break)
                    break;
            }
        }
    }

    static void tick();

    static void update_leds();

    static List& list() { return s_list; }

    static float absolute_position(size_t index);

    static size_t number_of_stations() { return s_list_size; }

    Station(Station const&) = delete;
    Station(Station&&) = delete;
    Station& operator=(Station const&) = delete;
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

    size_t number() const;
    size_t index() const;

    bool waiting_user_confirmation() const { return m_status == Status::ConfirmingScald or m_status == Status::ConfirmingAttacks; }
    bool waiting_user_input() const { return waiting_user_confirmation() or m_status == Status::Ready; }

public:
    pin_t button() const { return m_button_pin; }
    void set_button(pin_t pin);

    pin_t led() const { return m_led_pin; }
    void set_led(pin_t pin);

    bool is_button_held() const { return m_is_button_held; }
    void set_is_button_held(bool b) { m_is_button_held = b; }

    millis_t tick_button_was_pressed() const { return m_tick_button_was_pressed; }
    void set_tick_button_was_pressed(millis_t t) { m_tick_button_was_pressed = t; }

    bool recipe_was_cancelled() const { return m_recipe_was_cancelled; }
    void set_recipe_was_cancelled(bool b) { m_recipe_was_cancelled = b; }

    bool blocked() const { return m_blocked; }
    void set_blocked(bool);

    Status status() const { return m_status; }
    void set_status(Status, std::optional<uint32_t> receita_id = std::nullopt);

private:
    // initialized out of line porque nesse momento a classe 'Station' é incompleta
    static List s_list;

    static inline size_t s_list_size = 0;

private:
    Station() = default;

    // status, usado pelo app
    Status m_status = Status::Free;

    // o pin físico do nosso botão
    pin_t m_button_pin = 0;

    // o pin físico da nossa led
    pin_t m_led_pin = 0;

    // o ultimo tick em que o button foi apertado
    // usado para cancelar uma recipe
    millis_t m_tick_button_was_pressed = 0;

    // usado como um debounce para ativar uma station somente quando o botão é solto
    bool m_is_button_held = false;

    // se a recipe acabou de ser cancelada e o button ainda nao foi solto
    bool m_recipe_was_cancelled = false;

    // no app voce tem a possibilidade de bloquear um boca
    bool m_blocked = false;
};
}