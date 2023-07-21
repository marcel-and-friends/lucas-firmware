#pragma once

#include <lucas/Filtros.h>

namespace lucas::util {
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

}
