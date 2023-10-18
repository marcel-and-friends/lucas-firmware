#pragma once

#include <chrono>
#include <ratio>
#include <src/HAL/shared/Marduino.h>
#include <wiring_time.h>
#include <lucas/util/util.h>

namespace lucas::util {
class Timer {
public:
    static Timer started() {
        Timer t;
        t.start();
        return t;
    }

    void start() {
        m_tick = millis();
    }

    void stop() {
        m_tick = 0;
    }

    void restart() {
        start();
    }

    void toggle_based_on(bool condition) {
        if (is_active() and not condition) {
            stop();
        } else if (not is_active() and condition) {
            start();
        }
    }

    void toggle_based_on(Fn<bool> auto&& condition) {
        if (is_active() and not std::invoke(FWD(condition))) {
            stop();
        } else if (not is_active() and std::invoke(FWD(condition))) {
            start();
        }
    }

    bool has_elapsed(const auto duration) const {
        return is_active() and elapsed() >= duration;
    }

    template<typename T = chrono::milliseconds>
    T elapsed() const {
        return is_active() ? T{ millis() - m_tick } : T{};
    }

    bool is_active() const {
        return m_tick != 0;
    }

public:
    bool operator>(const auto duration) const {
        return elapsed() > duration;
    }

    bool operator>=(const auto duration) const {
        return elapsed() >= duration;
    }

    bool operator<(const auto duration) const {
        return elapsed() < duration;
    }

    bool operator<=(const auto duration) const {
        return elapsed() <= duration;
    }

private:
    millis_t m_tick = 0;
};
}
