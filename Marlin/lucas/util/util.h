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

inline float step_ratio() {
    constexpr float steps_per_mm = 44.5f;
    return planner.settings.axis_steps_per_mm[X_AXIS] / steps_per_mm;
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

static constexpr millis_t MARGEM_DE_VIAGEM = 1000;

template<typename FN, typename... Args>
concept IterCallback = std::invocable<FN, Args...> && std::same_as<std::invoke_result_t<FN, Args...>, Iter>;

#define FWD(x) std::forward<decltype(x)>(x)
}
