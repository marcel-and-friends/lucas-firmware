#pragma once

#include <lucas/core/TemporaryFilter.h>
#include <src/MarlinCore.h>
#include <chrono>
#include <string_view>
#include <concepts>

namespace lucas {
namespace chrono = std::chrono;
using namespace std::literals;
namespace util {
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

inline const char* fmt(const char* fmt, auto... args) {
    static char buffer[256] = {};
    sprintf(buffer, fmt, args...);
    return buffer;
}

const char* ff(const char* str, float valor);

bool is_button_held(pin_t pin);

float normalize(float v, float min, float max);

void idle_for(const auto duration, core::Filter filtros = core::Filter::None) {
    core::TemporaryFilter f{ filtros };
    const auto ms = chrono::duration_cast<chrono::milliseconds>(duration).count();
    const auto comeco = millis();
    while (millis() - comeco < ms)
        idle();
}

template<millis_t INTERVAL>
bool elapsed(millis_t last) {
    return millis() - last >= INTERVAL;
}

inline void idle_while(Fn<bool> auto&& callback, core::Filter filters = core::Filter::None) {
    core::TemporaryFilter f{ filters };
    while (std::invoke(FWD(callback)))
        idle();
}

inline void idle_until(Fn<bool> auto&& callback, core::Filter filters = core::Filter::None) {
    idle_while(std::not_fn(FWD(callback)), filters);
}

#define LOG SERIAL_ECHOLNPGM
#define LOG_ERR(...) LOG("", "ERRO: ", __VA_ARGS__);

#define every(interval_literal)                                                                                      \
    if ([] {                                                                                                         \
            static constexpr auto _interval = chrono::duration_cast<chrono::milliseconds>(interval_literal).count(); \
            static auto _s_last_tick = millis();                                                                     \
            const auto delta = millis() - _s_last_tick;                                                              \
            if (delta >= _interval) {                                                                                \
                _s_last_tick = millis();                                                                             \
                return delta - _interval <= 5;                                                                       \
            }                                                                                                        \
            return false;                                                                                            \
        }())
}
}
