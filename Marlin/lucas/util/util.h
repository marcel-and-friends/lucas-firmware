#pragma once

#include <lucas/tick.h>
#include <src/MarlinCore.h>
#include <src/module/planner.h>
#include <avr/dtostrf.h>
#include <type_traits>
#include <concepts>
#include <lucas/Filtros.h>

namespace lucas::util {
static constexpr millis_t MARGEM_DE_VIAGEM = 1000;

enum class Iter {
    Continue = 0,
    Break
};

template<typename T, typename Ret, typename... Args>
concept Fn = std::invocable<T, Args...> && std::same_as<std::invoke_result_t<T, Args...>, Ret>;

template<typename FN, typename... Args>
concept IterCallback = Fn<FN, Iter, Args...>;

#define FWD(x) std::forward<decltype(x)>(x)

inline const char* fmt(const char* fmt, auto... args) {
    static char buffer[256] = {};
    sprintf(buffer, fmt, args...);
    return buffer;
}

// isso aqui é uma desgraça mas é o que tem pra hoje
inline const char* ff(const char* str, float valor) {
    char buffer[16] = {};
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

inline float distancia_primeira_estacao() {
    return 80.f / step_ratio_x();
}

inline float distancia_entre_estacoes() {
    return 160.f / step_ratio_x();
}

class FiltroUpdatesTemporario {
public:
    FiltroUpdatesTemporario(Filtros filtros) {
        m_backup = filtros_atuais();
        filtrar_updates(filtros | m_backup);
    }

    ~FiltroUpdatesTemporario() {
        filtrar_updates(m_backup);
    }

private:
    Filtros m_backup;
};

inline void aguardar_por(millis_t tempo, Filtros filtros = Filtros::Nenhum) {
    FiltroUpdatesTemporario f{ filtros };

    const auto comeco = millis();
    while (millis() - comeco < tempo)
        idle();
}

inline void aguardar_enquanto(Fn<bool> auto&& callback, Filtros filtros = Filtros::Nenhum) {
    FiltroUpdatesTemporario f{ filtros };

    while (std::invoke(FWD(callback)))
        idle();
}

inline void aguardar_ate(Fn<bool> auto&& callback, Filtros filtros = Filtros::Nenhum) {
    aguardar_enquanto(std::not_fn(FWD(callback)), filtros);
}
}
