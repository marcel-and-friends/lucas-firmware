#pragma once

#include <array>
#include <src/MarlinCore.h>
#include <lucas/util/util.h>
#include <ArduinoJson.h>

namespace lucas::info {
struct Report {
    using Callback = void (*)(millis_t, JsonObject);
    using CallbackCondicao = bool (*)();

    const char* nome = "";
    millis_t intervalo = 0;
    millis_t ultimo_tick_reportado = 0;
    Callback callback = nullptr;
    CallbackCondicao condicao = nullptr;

    static void make(const char* nome, millis_t intervalo, Callback, CallbackCondicao condicao = nullptr);

    static void for_each(util::IterCallback<Report&> auto&& callback) {
        if (!s_num_reports)
            return;

        for (size_t i = 0; i < s_num_reports; ++i)
            if (std::invoke(FWD(callback), s_reports[i]) == util::Iter::Break)
                break;
    }

    millis_t delta(millis_t tick) const;

    using Lista = std::array<Report, 5>;

private:
    static Lista s_reports;
    static inline size_t s_num_reports = 0;
};
}
