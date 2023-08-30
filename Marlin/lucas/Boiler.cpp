#include "Boiler.h"
#include <lucas/lucas.h>
#include <lucas/info/info.h>
#include <lucas/sec/sec.h>
#include <src/module/temperature.h>

namespace lucas {
enum Pin {
    WaterLevelAlarm = PC5,
    Resistance = HEATER_BED_PIN
};

static bool s_alarm_triggered = false;

void Boiler::setup() {
    pinMode(Pin::WaterLevelAlarm, INPUT);
    /* go back to this when everything is setup
    s_alarm_triggered = READ(Pin::WaterLevelAlarm);
    attachInterrupt(
        digitalPinToInterrupt(Pin::WaterLevelAlarm),
        +[] {
            s_alarm_triggered = READ(Pin::WaterLevelAlarm);
        },
        CHANGE);
    */
    attachInterrupt(
        digitalPinToInterrupt(Pin::WaterLevelAlarm),
        +[] {
            s_alarm_triggered = not s_alarm_triggered;
        },
        FALLING);

    if (s_alarm_triggered) {
        info::send(
            info::Event::Boiler,
            [](JsonObject o) {
                o["filling"] = true;
            });

        util::idle_while([] { return s_alarm_triggered; });

        info::send(
            info::Event::Boiler,
            [](JsonObject o) {
                o["filling"] = false;
            });
    }
}

void Boiler::tick() {
    if (s_alarm_triggered)
        sec::raise_error(sec::Reason::WaterLevelAlarm);

    constexpr auto MAXIMUM_TEMPERATURE = 105.f;
    const auto temperature = this->temperature();
    if (temperature >= MAXIMUM_TEMPERATURE)
        sec::raise_error(sec::Reason::MaxTemperatureReached);

    const auto target = target_temperature();
    if (not target) {
        control_resistance(LOW);
        return;
    }

    control_resistance(temperature < (target - m_hysteresis));

    // security checks and stuff!

    { // out of the valid temperature range for too long
        if (m_reached_target_temperature) {
            constexpr auto VALID_TEMPERATURE_RANGE = 5.f;
            const auto in_target_range = std::abs(target - temperature) <= VALID_TEMPERATURE_RANGE;
            m_outside_target_range_timer.toggle_based_on(not in_target_range);
            if (m_outside_target_range_timer.has_elapsed(5min))
                sec::raise_error(sec::Reason::TemperatureOutOfRange);
        }
    }
}

float Boiler::temperature() const {
    return thermalManager.degBed();
}

void Boiler::set_target_temperature_and_wait(s32 target) {
    set_target_temperature(target);
    if (not target)
        return;

    m_reached_target_temperature = false;
    util::idle_until([this,
                      in_range_timer = util::Timer(),
                      last_checked_temperature = temperature()] mutable {
        every(5s) {
            info::send(
                info::Event::Boiler,
                [](JsonObject o) {
                    o["currentTemp"] = Boiler::the().temperature();
                });
        }

        const auto temperature = this->temperature();
        every(2min) {
            // if the temperature isn't going up let's kill ourselves NOW
            if (last_checked_temperature > temperature or
                temperature - last_checked_temperature < 1.f)
                sec::raise_error(sec::Reason::TemperatureNotChanging);

            last_checked_temperature = temperature;
        }

        in_range_timer.toggle_based_on(std::abs(m_target_temperature - temperature) <= m_hysteresis);
        // stop when we have staid in the valid range for 5 seconds
        m_reached_target_temperature = in_range_timer.has_elapsed(5s);
        return m_reached_target_temperature;
    });

    // and we're done! inform the app of our totaly real temperature
    info::send(
        info::Event::Boiler,
        [](JsonObject o) {
            o["currentTemp"] = Boiler::the().target_temperature();
        });

    m_hysteresis = LOW_HYSTERESIS;
    LOG_IF(LogCalibration, "temperatura desejada foi atingida");
}

void Boiler::set_target_temperature(s32 target) {
    m_target_temperature = target;
    if (not m_target_temperature)
        return;

    const auto delta = std::abs(target - temperature());
    if (temperature() > target) {
        m_hysteresis = LOW_HYSTERESIS;
    } else {
        m_hysteresis = delta < HIGH_HYSTERESIS ? LOW_HYSTERESIS : HIGH_HYSTERESIS;
    }
    LOG_IF(LogCalibration, "temperatura target setada - [target = ", target, " | hysteresis = ", m_hysteresis, "]");
}

void Boiler::control_resistance(bool state) {
    WRITE(Pin::Resistance, state);
}

void Boiler::turn_off_resistance() {
    control_resistance(LOW);
    set_target_temperature(0);
}
}
