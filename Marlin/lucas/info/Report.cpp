#include "Report.h"
#include <lucas/lucas.h>

namespace lucas::info {
Report::Lista Report::s_reports = {};

void Report::make(char const* nome, millis_t intervalo, Callback callback, CallbackCondicao condicao) {
    if (s_num_reports >= s_reports.size()) {
        LOG_ERR("muitos reports!!");
        return;
    }
    s_reports[s_num_reports++] = { nome, intervalo, 0, callback, condicao };
}

millis_t Report::delta(millis_t tick) const {
    if (ultimo_tick_reportado >= tick)
        return 0;
    return tick - ultimo_tick_reportado;
}
}
