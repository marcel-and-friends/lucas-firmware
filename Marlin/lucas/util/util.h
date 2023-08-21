#pragma once

#include <lucas/tick.h>
#include <lucas/core/TemporaryFilter.h>
#include <src/MarlinCore.h>
#include <src/module/planner.h>
#include <avr/dtostrf.h>
#include <type_traits>
#include <concepts>

namespace lucas::util {
constexpr millis_t TRAVEL_MARGIN = 1000;

enum class Iter {
    Continue = 0,
    Break
};

template<typename T, typename Ret, typename... Args>
concept Fn = std::invocable<T, Args...> and std::same_as<std::invoke_result_t<T, Args...>, Ret>;

template<typename FN, typename... Args>
concept IterFn = Fn<FN, Iter, Args...>;

#define FWD(x) std::forward<decltype(x)>(x)

inline char const* fmt(char const* fmt, auto... args) {
    static char buffer[256] = {};
    sprintf(buffer, fmt, args...);
    return buffer;
}

char const* ff(char const* str, float valor);

bool is_button_held(int pin);

constexpr float MS_PER_MM = 12.41f;
constexpr float DEFAULT_STEPS_PER_MM_X = 44.5f;
constexpr float DEFAULT_STEPS_PER_MM_Y = 26.5f;

float step_ratio_x();
float step_ratio_y();

float first_station_abs_pos();
float distance_between_each_station();

float normalize(float v, float min, float max);

void wait_for(millis_t tempo, Filters filtros = Filters::None);

inline void wait_while(Fn<bool> auto&& callback, Filters filtros = Filters::None) {
    core::TemporaryFilter f{ filtros };
    while (std::invoke(FWD(callback)))
        idle();
}

inline void wait_until(Fn<bool> auto&& callback, Filters filtros = Filters::None) {
    wait_while(std::not_fn(FWD(callback)), filtros);
}

#define LOG SERIAL_ECHOLNPGM
#define LOG_ERR(...) LOG("", "ERRO: ", __VA_ARGS__);
}
