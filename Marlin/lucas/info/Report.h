#pragma once

#include <array>
#include <src/MarlinCore.h>
#include <lucas/util/util.h>
#include <ArduinoJson.h>

namespace lucas::info {
struct Report {
    using Callback = void (*)(JsonObject);
    using CallbackCondicao = bool (*)();

    const char* nome = "";
    millis_t interval = 0;
    millis_t last_reported_tick = 0;
    Callback callback = nullptr;
    CallbackCondicao condition = nullptr;

    static void make(const char* nome, millis_t interval, Callback, CallbackCondicao condition = nullptr);

    static void for_each(util::IterFn<Report&> auto&& callback) {
        if (not s_num_reports)
            return;

        for (usize i = 0; i < s_num_reports; ++i)
            if (std::invoke(FWD(callback), s_reports[i]) == util::Iter::Break)
                break;
    }

    millis_t delta(millis_t tick) const;

    using List = std::array<Report, 1>;

private:
    static List s_reports;
    static inline usize s_num_reports = 0;
};
}
