#pragma once

#include <lucas/util/Timer.h>
#include <lucas/util/Singleton.h>

namespace lucas {
class Boiler : public util::Singleton<Boiler> {
public:
    enum Pin {
        WaterLevelAlarm = PE12,
        // PIN_WILSON
        Resistance = PC14
        // Resistance = PA0
    };

    void setup();

    void setup_pins();

    void tick();

    float temperature() const;

    void inform_temperature_to_host();

    bool is_in_target_temperature_range() const;

    bool is_heating() const;

    s32 target_temperature() const { return m_target_temperature; }
    void update_target_temperature(s32);
    void update_and_reach_target_temperature(s32);

    void control_resistance(bool state);

    void turn_off_resistance();

    bool is_alarm_triggered() const { return s_alarm_triggered; }

private:
    void reset();

    static constexpr auto HYSTERESIS = 0.5f;
    static constexpr auto TARGET_TEMPERATURE_RANGE_WHEN_ABOVE_TARGET = 2.0f;
    static constexpr auto TARGET_TEMPERATURE_RANGE_WHEN_BELOW_TARGET = 1.0f;

    static inline bool s_alarm_triggered = false;

    s32 m_target_temperature = -1;

    bool m_reached_target_temperature = false;

    util::Timer m_outside_target_range_timer;

    util::Timer m_temperature_stabilized_timer;

    util::Timer m_heating_check_timer;

    float m_last_checked_heating_temperature = 0.f;
};
}
