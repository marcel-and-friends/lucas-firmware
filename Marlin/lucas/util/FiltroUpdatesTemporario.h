#pragma once

#include <lucas/Filtros.h>
#include <src/core/serial.h>

namespace lucas::util {
class FiltroUpdatesTemporario {
public:
    FiltroUpdatesTemporario(Filtros filtros) {
        m_backup = filtros_atuais();
        aplicar_filtro(filtros | m_backup);
    }

    ~FiltroUpdatesTemporario() {
        aplicar_filtro(m_backup);
    }

private:
    Filtros m_backup;
};
}
