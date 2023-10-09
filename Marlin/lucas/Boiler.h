#pragma once

#include <lucas/util/Timer.h>
#include <lucas/util/Singleton.h>

namespace lucas {
class Boiler : public util::Singleton<Boiler> {
public:
    void setup();

    void tick();

    float temperature() const;

    float hysteresis() const { return m_hysteresis; }
    void set_hysteresis(float f) { m_hysteresis = f; }

    void inform_temperature_to_host();

    s32 target_temperature() const { return m_target_temperature; }
    void set_target_temperature(s32);
    void set_target_temperature_and_wait(s32);

    void control_resistance(bool state);

    void turn_off_resistance();

    bool is_alarm_triggered() const { return s_alarm_triggered; }

private:
    static constexpr auto HIGH_HYSTERESIS = 1.5f;
    static constexpr auto LOW_HYSTERESIS = 0.5f;

    static inline bool s_alarm_triggered = false;

    float m_hysteresis = 0.f;

    s32 m_target_temperature = 0;

    bool m_reached_target_temperature = false;

    util::Timer m_outside_target_range_timer;
};
}
