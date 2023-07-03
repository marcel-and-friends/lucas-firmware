#pragma once

#include <src/MarlinCore.h>
#include <src/module/planner.h>
#include <avr/dtostrf.h>
#include <type_traits>
#include <concepts>

namespace lucas::util {
inline const char* fmt(const char* fmt, auto... args) {
    static char buffer[128] = {};
    sprintf(buffer, fmt, args...);
    return buffer;
}

// isso aqui é uma desgraça mas é o que tem pra hoje
inline const char* ff(const char* str, float valor) {
    static char buffer[16] = {};
    dtostrf(valor, 0, 2, buffer);
    return fmt(str, buffer);
}

inline bool segurando(int pino) {
    return READ(pino) == false;
}

constexpr float MS_POR_MM = 12.41f;
constexpr float DEFAULT_STEPS_POR_MM_X = 44.5f;
constexpr float DEFAULT_STEPS_POR_MM_Y = 26.5f;

inline float step_ratio_x() {
    return planner.settings.axis_steps_per_mm[X_AXIS] / DEFAULT_STEPS_POR_MM_X;
}

inline float step_ratio_y() {
    return planner.settings.axis_steps_per_mm[Y_AXIS] / DEFAULT_STEPS_POR_MM_Y;
}

inline void aguardar_por(millis_t tempo) {
    const auto comeco = millis();
    while (millis() - comeco < tempo)
        idle();
}

enum class Iter {
    Continue = 0,
    Break
};

static constexpr millis_t MARGEM_DE_VIAGEM = 2000;

template<typename FN, typename... Args>
concept IterCallback = std::invocable<FN, Args...> && std::same_as<std::invoke_result_t<FN, Args...>, Iter>;

#define FWD(x) std::forward<decltype(x)>(x)
}
