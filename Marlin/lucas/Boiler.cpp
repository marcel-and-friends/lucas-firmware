#include "Boiler.h"
#include <lucas/lucas.h>
#include <lucas/RecipeQueue.h>
#include <lucas/info/info.h>
#include <lucas/sec/sec.h>
#include <src/module/temperature.h>
#include <utility>

namespace lucas {
static bool s_alarm_triggered = false;

void Boiler::setup() {
    setup_pins();
    /* go back to this when everything is setup
    s_alarm_triggered = digitalRead(Pin::WaterLevelAlarm);
    attachInterrupt(
        digitalPinToInterrupt(Pin::WaterLevelAlarm),
        +[] {
            s_alarm_triggered = digitalRead(Pin::WaterLevelAlarm);
        },
        CHANGE);
    attachInterrupt(
        digitalPinToInterrupt(Pin::WaterLevelAlarm),
        +[] {
            s_alarm_triggered = not s_alarm_triggered;
        },
        FALLING);
    */

    if (s_alarm_triggered) {
        info::send(
            info::Event::Boiler,
            [](JsonObject o) {
                o["filling"] = true;
            });

        util::idle_while(
            [timeout = util::Timer::started()] {
                if (timeout >= 25min)
                    sec::raise_error(sec::Error::WaterLevelAlarm);

                return s_alarm_triggered;
            });

        info::send(
            info::Event::Boiler,
            [](JsonObject o) {
                o["filling"] = false;
            });
    }
}

void Boiler::setup_pins() {
    pinMode(Pin::Resistance, OUTPUT);
    digitalWrite(Boiler::Pin::Resistance, LOW);

    pinMode(Pin::WaterLevelAlarm, OUTPUT);
    analogWrite(Boiler::Pin::WaterLevelAlarm, LOW);
    pinMode(Pin::WaterLevelAlarm, INPUT);
}

void Boiler::tick() {
    if (s_alarm_triggered)
        sec::raise_error(sec::Error::WaterLevelAlarm);

    every(5s) {
        inform_temperature_to_host();
    }

    constexpr auto MAXIMUM_TEMPERATURE = 105.f;
    const auto temperature = this->temperature();
    if (temperature >= MAXIMUM_TEMPERATURE)
        sec::raise_error(sec::Error::MaxTemperatureReached);

    if (not m_target_temperature) {
        control_resistance(false);
        return;
    }

    control_resistance(temperature < (m_target_temperature - m_hysteresis));

    if (not CFG(MaintenanceMode)) {
        { // out of the valid temperature range for too long
            if (m_reached_target_temperature) {
                constexpr auto VALID_TEMPERATURE_RANGE = 5.f;
                const auto in_target_range = std::abs(m_target_temperature - temperature) <= VALID_TEMPERATURE_RANGE;
                m_outside_target_range_timer.toggle_based_on(not in_target_range);
                if (m_outside_target_range_timer >= 5min)
                    sec::raise_error(sec::Error::TemperatureOutOfRange);
            }
        }
    }
}

float Boiler::temperature() const {
    return CFG(GigaMode) ? 94 : thermalManager.degBed();
}

void Boiler::inform_temperature_to_host() {
    info::send(
        info::Event::Boiler,
        [this](JsonObject o) {
            o["reachingTargetTemp"] = m_target_temperature and not m_reached_target_temperature;
            o["currentTemp"] = temperature();
        });
}

void Boiler::set_target_temperature_and_wait(s32 target) {
    set_target_temperature(target);
    if (not target)
        return;

    if (CFG(GigaMode)) {
        LOG(m_cooling ? "resfri" : "esquent", "ando boiler no modo giga...");
        util::idle_for(m_cooling ? 120s : 60s);
    } else {
        m_reached_target_temperature = false;
        util::idle_until([this,
                          in_range_timer = util::Timer(),
                          last_checked_temperature = temperature()] mutable {
            const auto temperature = this->temperature();
            if (not m_cooling) {
                every(2min) {
                    // if the temperature isn't going up let's kill ourselves NOW
                    if (last_checked_temperature > temperature or
                        temperature - last_checked_temperature < 1.f)
                        sec::raise_error(sec::Error::TemperatureNotChanging);

                    last_checked_temperature = temperature;
                }
            }

            in_range_timer.toggle_based_on(std::abs(m_target_temperature - temperature) <= m_hysteresis);
            // stop when we have staid in the valid range for 5 seconds
            m_reached_target_temperature = in_range_timer >= 5s;
            return m_reached_target_temperature;
        });
    }

    m_hysteresis = LOW_HYSTERESIS;
    LOG_IF(LogCalibration, "temperatura desejada foi atingida");
}

void Boiler::set_target_temperature(s32 target) {
    const auto old_target_temperature = std::exchange(m_target_temperature, target);
    if (not m_target_temperature)
        return;

    m_cooling = m_target_temperature < (old_target_temperature ?: temperature());

    const auto delta = std::abs(target - temperature());
    if (temperature() > target) {
        m_hysteresis = LOW_HYSTERESIS;
    } else {
        m_hysteresis = delta < HIGH_HYSTERESIS ? LOW_HYSTERESIS : HIGH_HYSTERESIS;
    }
    LOG_IF(LogCalibration, "temperatura target setada - [target = ", target, " | hysteresis = ", m_hysteresis, "]");
}

void Boiler::control_resistance(bool state) {
    digitalWrite(Pin::Resistance, state);
}

void Boiler::turn_off_resistance() {
    control_resistance(false);
    set_target_temperature(0);
}
}
