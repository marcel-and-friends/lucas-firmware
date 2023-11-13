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

    turn_off_resistance();

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

    constexpr auto MAXIMUM_TEMPERATURE = 105.f;
    if (temperature() >= MAXIMUM_TEMPERATURE)
        sec::raise_error(sec::Error::MaxTemperatureReached);

    if (not m_target_temperature)
        return;

    if (CFG(GigaMode)) {
        every(5s) {
            inform_temperature_to_host();
        }
        return;
    }

    control_resistance(temperature() < (m_target_temperature - HYSTERESIS));

    if (not m_reached_target_temperature) {
        m_temperature_stabilized_timer.toggle_based_on(is_in_target_temperature_range());
        m_reached_target_temperature = m_temperature_stabilized_timer >= 5s;
        if (is_heating()) {
            if (not m_heating_check_timer.is_active()) {
                m_last_checked_heating_temperature = temperature();
                m_heating_check_timer.start();
            }

            if (m_heating_check_timer >= 2min) {
                if (temperature() - m_last_checked_heating_temperature < 0.5f)
                    sec::raise_error(sec::Error::TemperatureNotChanging);

                m_last_checked_heating_temperature = temperature();
                m_heating_check_timer.restart();
            }
        } else {
            m_heating_check_timer.stop();
        }
    } else {
        constexpr auto MAX_TEMPERATURE_RANGE_AFTER_REACHING_TARGET = 5.f;
        const auto in_target_range = std::abs(m_target_temperature - temperature()) <= MAX_TEMPERATURE_RANGE_AFTER_REACHING_TARGET;
        m_outside_target_range_timer.toggle_based_on(not in_target_range);
        if (m_outside_target_range_timer >= 5min)
            sec::raise_error(sec::Error::TemperatureOutOfRange);
    }

    every(5s) {
        inform_temperature_to_host();
    }
}

float Boiler::temperature() const {
    return CFG(GigaMode) ? 94 : thermalManager.degBed();
}

bool Boiler::is_in_target_temperature_range() const {
    const auto delta = temperature() - m_target_temperature;
    return std::abs(delta) <= (delta > 0.f
                                   ? TARGET_TEMPERATURE_RANGE_WHEN_ABOVE_TARGET
                                   : TARGET_TEMPERATURE_RANGE_WHEN_BELOW_TARGET);
}

bool Boiler::is_heating() const {
    return temperature() < m_target_temperature;
}

void Boiler::inform_temperature_to_host() {
    info::send(
        info::Event::Boiler,
        [this](JsonObject o) {
            o["reachingTargetTemp"] = not is_in_target_temperature_range();
            o["heating"] = temperature() < m_target_temperature;
            o["currentTemp"] = temperature();
        });
}

void Boiler::update_and_reach_target_temperature(s32 target) {
    update_target_temperature(target);
    if (not target)
        return;

    if (CFG(GigaMode)) {
        LOG("esquentando boiler no modo giga...");
        util::idle_for(1min);
        return;
    }

    if (CFG(GigaMode)) {
        LOG(is_heating() ? "esquent" : "resfri", "ando boiler no modo giga...");
        util::idle_for(is_heating() ? 120s : 60s);
    } else {
        util::idle_until([this] { return m_reached_target_temperature; });
    }

    inform_temperature_to_host();

    LOG_IF(LogCalibration, "temperatura desejada foi atingida");
}

void Boiler::update_target_temperature(s32 target) {
    if (m_target_temperature and target == m_target_temperature)
        return;

    m_target_temperature = target;
    reset();

    if (not m_target_temperature) {
        control_resistance(false);
        return;
    }

    inform_temperature_to_host();

    LOG_IF(LogCalibration, "temperatura target setada - [target = ", target, " | hysteresis = ", HYSTERESIS, "]");
}

void Boiler::control_resistance(bool state) {
    digitalWrite(Pin::Resistance, state);
}

void Boiler::turn_off_resistance() {
    update_target_temperature(0);
}

void Boiler::reset() {
    m_reached_target_temperature = false;

    m_outside_target_range_timer.stop();
    m_temperature_stabilized_timer.stop();
    m_heating_check_timer.stop();

    m_last_checked_heating_temperature = 0.f;
}
}
