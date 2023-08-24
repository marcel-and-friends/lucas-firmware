#include "Boiler.h"

#include <lucas/lucas.h>
#include <lucas/info/info.h>
#include <lucas/sec/sec.h>
#include <src/module/temperature.h>

namespace lucas {
enum Pin {
    CoelAlarmOutput = PD11,
    CoelAlarmInput = PB2
};

static bool s_alarm_triggered = false;

void Boiler::setup() {
    pinMode(Pin::CoelAlarmInput, INPUT);
    attachInterrupt(
        digitalPinToInterrupt(Pin::CoelAlarmInput),
        +[] {
            s_alarm_triggered = not s_alarm_triggered;
        },
        FALLING);

    pinMode(Pin::CoelAlarmOutput, OUTPUT);
    digitalWrite(Pin::CoelAlarmOutput, HIGH);

    if (s_alarm_triggered) {
        info::send(info::Event::Boiler, [](JsonObject o) {
            o["filling"] = true;
        });
        util::idle_while([] { return s_alarm_triggered; });
        info::send(info::Event::Boiler, [](JsonObject o) {
            o["filling"] = false;
        });
    }
}

void Boiler::tick() {
    if (s_alarm_triggered)
        sec::raise_error(sec::Reason::BoilerAlarm, [] { return s_alarm_triggered; });
}

int Boiler::target_temperature() const {
    return thermalManager.degTargetBed();
}

void Boiler::set_target_temperature_and_wait(int target) {
    set_target_temperature(target);
    const auto initial_boiler_temp = thermalManager.degBed();
    if (initial_boiler_temp >= target)
        return;

    util::idle_until([&] {
        constexpr auto LOG_INTERVAL = 5000;
        static auto s_last_log_tick = 0;

        const auto current_temp = thermalManager.degBed();
        if (util::elapsed<LOG_INTERVAL>(s_last_log_tick)) {
            info::send(info::Event::Boiler, [current_temp](JsonObject o) {
                o["currentTemp"] = current_temp;
            });
            s_last_log_tick = millis();
        }

        const auto in_acceptable_range = std::abs(target - current_temp) <= m_hysteresis;
        constexpr auto TIME_TO_STABILIZE = 5000;
        static auto s_tick_temperature_started_to_stabilize = 0;
        if (s_tick_temperature_started_to_stabilize == 0) {
            if (in_acceptable_range) {
                s_tick_temperature_started_to_stabilize = millis();
            }
        } else {
            if (in_acceptable_range) {
                if (util::elapsed<TIME_TO_STABILIZE>(s_tick_temperature_started_to_stabilize)) {
                    // boiler is in the acceptable hysteresis range for long enough, stop idling
                    return true;
                }
            } else {
                s_tick_temperature_started_to_stabilize = 0;
            }
        }
        return false;
    });

    info::send(info::Event::Boiler, [target](JsonObject o) {
        o["currentTemp"] = target;
    });

    LOG_IF(LogCalibration, "temperatura desejada foi atingida");

    m_hysteresis = FINAL_HYSTERESIS;
}

void Boiler::set_target_temperature(int target) {
    thermalManager.setTargetBed(target);

    const auto delta = std::abs(target - thermalManager.degBed());
    m_hysteresis = delta < INITIAL_HYSTERESIS ? FINAL_HYSTERESIS : INITIAL_HYSTERESIS;

    LOG_IF(LogCalibration, "temperatura target setada - [target = ", target, " | hysteresis = ", m_hysteresis, "]");
}

void Boiler::disable_heater() {
    thermalManager.setTargetBed(0);
}
}
