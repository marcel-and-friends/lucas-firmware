#pragma once

#include <lucas/Filters.h>
#include <src/core/serial.h>

namespace lucas::util {
class TemporaryFilter {
public:
    TemporaryFilter(Filters filtros) {
        m_backup = current_filters();
        apply_filters(filtros | m_backup);
    }

    ~TemporaryFilter() {
        apply_filters(m_backup);
    }

private:
    Filters m_backup;
};
}
