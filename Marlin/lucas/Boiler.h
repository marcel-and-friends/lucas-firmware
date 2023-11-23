#pragma once

#include <lucas/util/Timer.h>
#include <lucas/core/core.h>
#include <lucas/storage/storage.h>
#include <lucas/util/Singleton.h>

namespace lucas {
class Boiler : public util::Singleton<Boiler> {
public:
    enum Pin {
        WaterLevelAlarm = PC4,
        Resistance = PC14
    };

    void setup();

    void setup_pins();

    void tick();

    float temperature() const;

    std::optional<s32> stored_target_temperature() const;

    static void inform_temperature_status() {
        core::inform_calibration_status();
        the().inform_temperature_to_host();
    }

    void inform_temperature_to_host();

    bool is_in_target_temperature_range() const;

    bool is_heating() const;

    s32 target_temperature() const { return m_target_temperature; }
    void update_target_temperature(std::optional<s32>);
    void update_and_reach_target_temperature(std::optional<s32>);

    void control_resistance(bool state);

    void turn_off_resistance();

    bool is_alarm_triggered() const;

private:
    void reset();

    static constexpr auto HYSTERESIS = 0.5f;
    static constexpr auto TARGET_TEMPERATURE_RANGE_WHEN_ABOVE_TARGET = 2.0f;
    static constexpr auto TARGET_TEMPERATURE_RANGE_WHEN_BELOW_TARGET = 1.0f;

    s32 m_target_temperature = 0;

    storage::Handle m_storage_handle;

    bool m_should_wait_for_boiler_to_fill = false;

    bool m_reached_target_temperature = false;

    util::Timer m_outside_target_range_timer;

    util::Timer m_temperature_stabilized_timer;

    util::Timer m_heating_check_timer;

    float m_last_checked_heating_temperature = 0.f;
};
}
