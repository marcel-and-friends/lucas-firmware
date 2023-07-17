#pragma once

#include <cstdint>

namespace lucas {
enum class Filtros : uint32_t {
    Nenhum = 0,

    Fila = 1 << 0,
    Estacao = 1 << 1,
    Bico = 1 << 2,
    Info = 1 << 3,
    SerialHooks = 1 << 4,

    Interacao = Fila | Estacao | SerialHooks,
    Todos = Fila | Estacao | Bico | Info | SerialHooks,
};

constexpr Filtros operator|(const Filtros a, const Filtros b) {
    return Filtros(uint32_t(a) | uint32_t(b));
}

constexpr Filtros operator&(const Filtros a, const Filtros b) {
    return Filtros(uint32_t(a) & uint32_t(b));
}

void filtrar_updates(Filtros filtro);

void resetar_filtros();

bool filtrado(const Filtros filtro);

Filtros filtros_atuais();
}
