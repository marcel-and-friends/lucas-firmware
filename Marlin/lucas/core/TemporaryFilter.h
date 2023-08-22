#pragma once

#include <lucas/TickFilter.h>
#include <src/core/serial.h>

namespace lucas::core {
class TemporaryFilter {
public:
    TemporaryFilter(TickFilter filtros) {
        m_backup = current_filters();
        apply_filters(filtros | m_backup);
    }

    ~TemporaryFilter() {
        apply_filters(m_backup);
    }

private:
    TickFilter m_backup;
};
}
