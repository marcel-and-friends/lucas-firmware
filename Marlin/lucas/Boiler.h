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
        Resistance = PB1
    };

    void setup();

    void setup_pins();

    void tick();

    float temperature() const;

    std::optional<s32> stored_target_temperature() const;

    s32 target_temperature() const { return m_target_temperature; }

    void update_target_temperature(std::optional<s32>);

    void update_and_reach_target_temperature(std::optional<s32>);

    bool is_alarm_triggered() const;

    void turn_off_resistance();

    bool is_in_coffee_making_temperature_range() const;

    static void inform_temperature_status() {
        core::inform_calibration_status();
        the().inform_temperature_to_host();
    }

private:
    void inform_temperature_to_host();

    void security_checks();

    void control_temperature();

    struct ModulateResistanceParams {
        f32 target_range_begin;
        f32 target_range_end;
        f32 max_strength;
        f32 min_strength;
    };

    void modulate_resistance(ModulateResistanceParams);

    void control_resistance(f32 force);

    void reset();

    enum class State {
        Heating,
        Stabilizing,
        None
    };

    State m_state = State::None;

    s32 m_target_temperature = 0;

    storage::Handle m_storage_handle;

    bool m_reaching_target_temp = false;

    bool m_should_wait_for_boiler_to_fill = false;

    float m_last_checked_heating_temperature = 0.f;
    util::Timer m_heating_check_timer;

    util::Timer m_outside_target_range_timer;
};
}
