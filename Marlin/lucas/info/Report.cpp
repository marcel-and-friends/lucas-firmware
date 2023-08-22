#include "Report.h"
#include <lucas/lucas.h>

namespace lucas::info {
Report::List Report::s_reports = {};

void Report::make(const char* nome, millis_t interval, Callback callback, CallbackCondicao condition) {
    if (s_num_reports >= s_reports.size()) {
        LOG_ERR("muitos reports!!");
        return;
    }
    s_reports[s_num_reports++] = { nome, interval, 0, callback, condition };
}

millis_t Report::delta(millis_t tick) const {
    if (last_reported_tick >= tick)
        return 0;
    return tick - last_reported_tick;
}
}
