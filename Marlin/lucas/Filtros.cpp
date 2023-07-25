#include "Filtros.h"

namespace lucas {
static Filtros s_filtro_updates = Filtros::Nenhum;

void aplicar_filtro(Filtros filtro) {
    s_filtro_updates = filtro;
}

void resetar_filtros() {
    s_filtro_updates = Filtros::Nenhum;
}

bool filtrado(const Filtros filtro) {
    return (s_filtro_updates & filtro) == filtro;
}

Filtros filtros_atuais() {
    return s_filtro_updates;
}
}
