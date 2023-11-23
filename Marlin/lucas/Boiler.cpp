#include "Boiler.h"
#include <lucas/core/core.h>
#include <lucas/info/info.h>
#include <lucas/sec/sec.h>
#include <lucas/RecipeQueue.h>
#include <src/module/temperature.h>
#include <utility>

namespace lucas {
void Boiler::setup() {
    setup_pins();
    turn_off_resistance();
    m_should_wait_for_boiler_to_fill = is_alarm_triggered();
    m_storage_handle = storage::register_handle_for_entry("temp", sizeof(m_target_temperature));
}

void Boiler::setup_pins() {
    pinMode(Pin::Resistance, OUTPUT);
    digitalWrite(Boiler::Pin::Resistance, LOW);

    pinMode(Pin::WaterLevelAlarm, INPUT_PULLUP);
}

static void filling_event(bool b) {
    info::send(
        info::Event::Boiler,
        [b](JsonObject o) {
            o["filling"] = b;
        });
}

static void filling_event() {
    filling_event(true);
}

static void wait_for_boiler_to_fill() {
    info::TemporaryCommandHook hook{
        info::Command::RequestInfoCalibration,
        &filling_event
    };

    filling_event(true);

    util::maintenance_idle_while(
        [timeout = util::Timer::started()] {
            if (timeout >= 15min)
                sec::raise_error(sec::Error::WaterLevelAlarm);

            return Boiler::the().is_alarm_triggered();
        });

    filling_event(false);
}

void Boiler::tick() {
    if (m_should_wait_for_boiler_to_fill) {
        m_should_wait_for_boiler_to_fill = false;
        wait_for_boiler_to_fill();
    }

    if (CFG(GigaMode)) {
        every(5s) {
            inform_temperature_to_host();
        }
        return;
    }

    if (is_alarm_triggered())
        sec::raise_error(sec::Error::WaterLevelAlarm);

    constexpr auto MAXIMUM_TEMPERATURE = 105.f;
    if (temperature() >= MAXIMUM_TEMPERATURE)
        sec::raise_error(sec::Error::MaxTemperatureReached);

    if (not m_target_temperature)
        return;

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

std::optional<s32> Boiler::stored_target_temperature() const {
    if (auto entry = storage::fetch_entry(m_storage_handle))
        return entry->read_binary<s32>();

    return std::nullopt;
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

void Boiler::update_and_reach_target_temperature(std::optional<s32> target) {
    update_target_temperature(target);
    if (not m_target_temperature)
        return;

    if (CFG(GigaMode)) {
        LOG("esquentando boiler no modo giga...");
        util::idle_for(1min);
        return;
    }

    util::idle_until([this] { return m_reached_target_temperature; });

    inform_temperature_to_host();

    LOG_IF(LogCalibration, "temperatura desejada foi atingida");
}

void Boiler::update_target_temperature(std::optional<s32> target) {
    if (target == m_target_temperature)
        return;

    reset();

    if (target == 0) {
        m_target_temperature = 0;
        control_resistance(false);
    } else {
        m_target_temperature = storage::create_or_update_entry(m_storage_handle, target, 94);
        inform_temperature_to_host();
    }

    LOG_IF(LogCalibration, "temperatura target setada - [target = ", m_target_temperature, "]");
}

void Boiler::control_resistance(bool state) {
    digitalWrite(Pin::Resistance, state);
}

void Boiler::turn_off_resistance() {
    update_target_temperature(0);
}

bool Boiler::is_alarm_triggered() const {
    return not digitalRead(Pin::WaterLevelAlarm);
}

void Boiler::reset() {
    m_reached_target_temperature = false;

    m_outside_target_range_timer.stop();
    m_temperature_stabilized_timer.stop();
    m_heating_check_timer.stop();

    m_last_checked_heating_temperature = 0.f;
}
}
