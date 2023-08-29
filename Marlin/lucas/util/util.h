#pragma once

#include <lucas/tick.h>
#include <lucas/core/TemporaryFilter.h>
#include <src/MarlinCore.h>
#include <type_traits>
#include <cstdint>
#include <cstddef>
#include <concepts>
#include <chrono>

namespace lucas {
using namespace std::chrono_literals;
using namespace std::string_view_literals;

using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using f32 = float;
using f64 = double;

using usize = size_t;
using ssize = std::make_signed_t<size_t>;
}

namespace chrono = std::chrono;

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

inline const char* fmt(const char* fmt, auto... args) {
    static char buffer[256] = {};
    sprintf(buffer, fmt, args...);
    return buffer;
}

const char* ff(const char* str, float valor);

bool is_button_held(int pin);

constexpr float MS_PER_MM = 12.41f;
constexpr float DEFAULT_STEPS_PER_MM_X = 22.f;
constexpr float DEFAULT_STEPS_PER_MM_Y = 12.f;

float step_ratio_x();
float step_ratio_y();

float first_station_abs_pos();
float distance_between_each_station();

float normalize(float v, float min, float max);

void idle_for(const auto duration, TickFilter filtros = TickFilter::None) {
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

inline void idle_while(Fn<bool> auto&& callback, TickFilter filters = TickFilter::None) {
    core::TemporaryFilter f{ filters };
    while (std::invoke(FWD(callback)))
        idle();
}

inline void idle_until(Fn<bool> auto&& callback, TickFilter filters = TickFilter::None) {
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
                return delta <= 5;                                                                                   \
            }                                                                                                        \
            return false;                                                                                            \
        }())
}
