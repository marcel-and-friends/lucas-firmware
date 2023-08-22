#include "Boiler.h"

#include <lucas/lucas.h>
#include <lucas/info/info.h>
#include <lucas/Spout.h>
#include <lucas/RecipeQueue.h>
#include <src/module/temperature.h>

namespace lucas {
enum Pin {
    CoelAlarmOutput = PD11,
    CoelAlarmInput = PB2
};

static bool s_alarm_triggered = false;

void Boiler::setup() {
    pinMode(Pin::CoelAlarmInput, INPUT);
    s_alarm_triggered = READ(Pin::CoelAlarmInput);
    attachInterrupt(
        digitalPinToInterrupt(Pin::CoelAlarmInput),
        +[] {
            s_alarm_triggered = READ(Pin::CoelAlarmInput);
        },
        CHANGE);

    pinMode(Pin::CoelAlarmOutput, OUTPUT);
    digitalWrite(Pin::CoelAlarmOutput, HIGH);

    if (s_alarm_triggered) {
        info::event("infoBoiler", [](JsonObject o) {
            o["enchendo"] = true;
        });

        util::idle_while([] { return s_alarm_triggered; });

        info::event("infoBoiler", [](JsonObject o) {
            o["enchendo"] = false;
        });
    }
}

void Boiler::tick() {
    if (s_alarm_triggered) {
        info::event("infoBoiler", [](JsonObject o) {
            o["alarm"] = true;
        });

        const auto old_temperature = thermalManager.degTargetBed();
        Boiler::the().disable_heater();
        Spout::the().end_pour();
        RecipeQueue::the().cancel_all_recipes();

        // spout's 'tick()' isn't filtered so that the pump's BRK is properly released after 'end_pour()'
        constexpr auto FILTER = TickFilter::All & ~TickFilter::Spout;
        util::idle_while([] { return s_alarm_triggered; }, FILTER);

        info::event("infoBoiler", [](JsonObject o) {
            o["alarm"] = false;
        });

        Boiler::the().set_target_temperature_and_wait(old_temperature);
    }
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
            info::event("infoBoiler", [current_temp](JsonObject o) {
                o["tempAtual"] = current_temp;
            });
            s_last_log_tick = millis();
        }

        const auto in_acceptable_range = std::abs(target - current_temp) >= m_hysteresis;
        constexpr auto TIME_TO_STABILIZE = 1000;
        static auto s_tick_temperature_started_to_stabilize = 0;
        if (s_tick_temperature_started_to_stabilize == 0) {
            if (in_acceptable_range)
                s_tick_temperature_started_to_stabilize = millis();
        } else {
            if (in_acceptable_range) {
                if (util::elapsed<TIME_TO_STABILIZE>(s_tick_temperature_started_to_stabilize))
                    // boiler is in the acceptable hysteresis range for long enough, stop idling
                    return true;
            } else {
                s_tick_temperature_started_to_stabilize = 0;
            }
        }
        return false;
    });

    info::event("infoBoiler", [target](JsonObject o) {
        o["tempAtual"] = target;
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
    LOG("resistencia foi desligada");
}
}
