#include "Boiler.h"

#include <lucas/lucas.h>
#include <lucas/info/info.h>
#include <lucas/Spout.h>
#include <lucas/RecipeQueue.h>
#include <src/module/temperature.h>

namespace lucas {
enum Pin {
    AlarmInput = PD11,
    AlarmOutput = PB2
};

static bool s_alarm_triggered = false;

void Boiler::setup() {
    pinMode(Pin::AlarmOutput, INPUT);
    s_alarm_triggered = READ(Pin::AlarmOutput);
    attachInterrupt(
        digitalPinToInterrupt(Pin::AlarmOutput),
        +[] {
            s_alarm_triggered = READ(Pin::AlarmOutput);
        },
        CHANGE);

    pinMode(Pin::AlarmInput, OUTPUT);
    digitalWrite(Pin::AlarmInput, HIGH);

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

        auto const old_temperature = thermalManager.degTargetBed();
        disable_heater();
        Spout::the().stop_pour();
        RecipeQueue::the().cancel_all_recipes();

        util::idle_while([] { return s_alarm_triggered; }, Filters::All & ~Filters::Spout);

        info::event("infoBoiler", [](JsonObject o) {
            o["alarm"] = false;
        });

        set_target_temperature_and_wait(old_temperature);
    }
}

void Boiler::set_target_temperature_and_wait(int target) {
    set_target_temperature(target);
    auto const temp_boiler = thermalManager.degBed();
    if (temp_boiler >= target)
        return;

    util::idle_while([&] {
        constexpr auto LOG_INTERVAL = 5000;
        static millis_t s_last_log_tick = 0;

        auto const current_temp = thermalManager.degBed();
        if (util::elapsed<LOG_INTERVAL>(s_last_log_tick)) {
            auto const progresso = util::normalize(current_temp, temp_boiler, float(target));
            info::event("infoBoiler", [progresso](JsonObject o) {
                o["progressoTemp"] = progresso;
            });
            s_last_log_tick = millis();
        }
        return std::abs(target - current_temp) >= m_hysteresis;
    });

    info::event("infoBoiler", [](JsonObject o) {
        o["progressoTemp"] = 1.f;
    });

    LOG_IF(LogCalibration, "temperatura desejada foi atingida");

    m_hysteresis = FINAL_HYSTERESIS;
}

void Boiler::set_target_temperature(int target) {
    thermalManager.setTargetBed(target);

    auto const delta = std::abs(target - thermalManager.degBed());
    m_hysteresis = delta < INITIAL_HYSTERESIS ? FINAL_HYSTERESIS : INITIAL_HYSTERESIS;

    LOG_IF(LogCalibration, "temperatura target setada - [target = ", target, " | hysteresis = ", m_hysteresis, "]");
}

void Boiler::disable_heater() {
    thermalManager.setTargetBed(0);
    LOG("resistencia foi desligada");
}
}
