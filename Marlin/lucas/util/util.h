#pragma once

#include <lucas/tick.h>
#include <lucas/util/FiltroUpdatesTemporario.h>
#include <src/MarlinCore.h>
#include <src/module/planner.h>
#include <avr/dtostrf.h>
#include <type_traits>
#include <concepts>

namespace lucas::util {
constexpr millis_t MARGEM_DE_VIAGEM = 1000;

constexpr float MS_POR_MM = 12.41f;
constexpr float DEFAULT_STEPS_POR_MM_X = 44.5f;
constexpr float DEFAULT_STEPS_POR_MM_Y = 26.5f;

enum class Iter {
    Continue = 0,
    Break
};

template<typename T, typename Ret, typename... Args>
concept Fn = std::invocable<T, Args...> && std::same_as<std::invoke_result_t<T, Args...>, Ret>;

template<typename FN, typename... Args>
concept IterFn = Fn<FN, Iter, Args...>;

#define FWD(x) std::forward<decltype(x)>(x)

inline const char* fmt(const char* fmt, auto... args) {
    static char buffer[256] = {};
    sprintf(buffer, fmt, args...);
    return buffer;
}

const char* ff(const char* str, float valor);

bool segurando(int pino);

float step_ratio_x();

float step_ratio_y();

float distancia_primeira_estacao();

float distancia_entre_estacoes();

float hysteresis();

void set_hysteresis(float valor);

void aguardar_por(millis_t tempo, Filtros filtros = Filtros::Nenhum);

inline void aguardar_enquanto(Fn<bool> auto&& callback, Filtros filtros = Filtros::Nenhum) {
    FiltroUpdatesTemporario f{ filtros };

    while (std::invoke(FWD(callback)))
        idle();
}

inline void aguardar_ate(Fn<bool> auto&& callback, Filtros filtros = Filtros::Nenhum) {
    aguardar_enquanto(std::not_fn(FWD(callback)), filtros);
}
}
