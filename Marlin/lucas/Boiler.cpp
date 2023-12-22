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
    // turn_off_resistance();
    m_should_wait_for_boiler_to_fill = is_alarm_triggered();
    m_storage_handle = storage::register_handle_for_entry("temp", sizeof(m_target_temperature));
}

void Boiler::setup_pins() {
    // pinMode(Pin::Resistance, OUTPUT);
    // control_resistance(0.f);

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
        if (is_alarm_triggered())
            wait_for_boiler_to_fill();
    }

    if (not CFG(GigaMode)) {
        // control_temperature();
        // security_checks();
    }

    if (m_target_temperature) {
        every(5s) {
            inform_temperature_to_host();
        }
    }
}

float Boiler::temperature() const {
    return CFG(GigaMode) ? 94 : thermalManager.degHotend(0);
}

std::optional<s32> Boiler::stored_target_temperature() const {
    if (auto entry = storage::fetch_entry(m_storage_handle))
        return entry->read_binary<s32>();

    return std::nullopt;
}

bool Boiler::is_in_coffee_making_temperature_range() const {
    const auto range_below = m_target_temperature - 3.0f;
    const auto range_above = m_target_temperature + 1.5f;
    return util::is_within(temperature(), range_below, range_above);
}

void Boiler::inform_temperature_to_host() {
    info::send(
        info::Event::Boiler,
        [this](JsonObject o) {
            o["reachingTargetTemp"] = m_reaching_target_temp ?: not is_in_coffee_making_temperature_range();
            o["heating"] = temperature() < m_target_temperature;
            o["currentTemp"] = temperature();
        });
}

void Boiler::update_and_reach_target_temperature(std::optional<s32> target) {
    if (target == m_target_temperature)
        return;

    m_reaching_target_temp = true;

    update_target_temperature(target);
    if (not m_target_temperature) {
        m_reaching_target_temp = false;
        return;
    }

    if (CFG(GigaMode)) {
        LOG("esquentando boiler no modo giga...");
        util::idle_for(1min);
    } else {
        thermalManager.wait_for_hotend(0, false);
    }

    m_reaching_target_temp = false;

    inform_temperature_to_host();
}

void Boiler::update_target_temperature(std::optional<s32> target) {
    if (target == m_target_temperature)
        return;

    if (target == 0) {
        // when resetting the target temp we don't save it to the storage
        m_target_temperature = 0;
        thermalManager.disable_all_heaters();
    } else {
        m_target_temperature = storage::create_or_update_entry(m_storage_handle, target, 94);
        thermalManager.setTargetHotend(m_target_temperature, 0);
    }

    m_state = temperature() < m_target_temperature ? State::Heating : State::Stabilizing;
    inform_temperature_to_host();
    reset();

    LOG_IF(LogCalibration, "temperatura target setada - [target = ", m_target_temperature, "]");
}

void Boiler::control_temperature() {
    if (not m_target_temperature) {
        control_resistance(0.f);
        return;
    }

    constexpr auto HEATING_TARGET_RANGE_BEGIN = 3.f;
    switch (m_state) {
    case State::Stabilizing:
        modulate_resistance({ .target_range_begin = 0.f,
                              .target_range_end = 0.5f,
                              .max_strength = 0.5f,
                              .min_strength = 0.f });

        if (temperature() <= m_target_temperature - HEATING_TARGET_RANGE_BEGIN)
            m_state = State::Heating;

        break;
    case State::Heating:
        modulate_resistance({ .target_range_begin = HEATING_TARGET_RANGE_BEGIN,
                              .target_range_end = 0.f,
                              .max_strength = 1.f,
                              .min_strength = 0.5f });

        if (temperature() >= m_target_temperature)
            m_state = State::Stabilizing;

        break;
    }
}

void Boiler::security_checks() {
    if (is_alarm_triggered()) {
        sec::raise_error(sec::Error::WaterLevelAlarm);
        return;
    }

    constexpr auto MAXIMUM_TEMPERATURE = 105.f;
    if (temperature() >= MAXIMUM_TEMPERATURE) {
        sec::raise_error(sec::Error::MaxTemperatureReached);
        return;
    }

    if (not m_target_temperature)
        return;

    if (not is_in_coffee_making_temperature_range()) {
        if (m_state == State::Heating) {
            if (not m_heating_check_timer.is_active()) {
                m_last_checked_heating_temperature = temperature();
                m_heating_check_timer.start();
            }

            if (m_heating_check_timer >= 2min) {
                if (temperature() - m_last_checked_heating_temperature < 0.5f) {
                    sec::raise_error(sec::Error::TemperatureNotChanging);
                    return;
                }

                m_last_checked_heating_temperature = temperature();
                m_heating_check_timer.restart();
            }
        } else {
            m_last_checked_heating_temperature = 0.f;
            m_heating_check_timer.stop();
        }
    } else {
        constexpr auto MAX_TEMPERATURE_RANGE_AFTER_REACHING_TARGET = 5.f;
        const auto in_target_range = std::abs(m_target_temperature - temperature()) <= MAX_TEMPERATURE_RANGE_AFTER_REACHING_TARGET;
        m_outside_target_range_timer.toggle_based_on(not in_target_range);
        if (m_outside_target_range_timer >= 5min) {
            sec::raise_error(sec::Error::TemperatureOutOfRange);
            return;
        }
    }
}

void Boiler::control_resistance(f32 force) {
    const auto digital_value = std::clamp(static_cast<s32>(force * 255.f), 0, 255);
    analogWrite(Pin::Resistance, digital_value);
}

void Boiler::modulate_resistance(ModulateResistanceParams params) {
    const auto range_below = m_target_temperature - params.target_range_begin;
    const auto range_above = m_target_temperature + params.target_range_end;
    auto strength = 0.f;
    if (util::is_within(temperature(), range_below, range_above)) {
        const auto norm = util::normalize(temperature(), range_below, range_above);
        strength = std::lerp(params.max_strength, params.min_strength, norm);
    } else {
        strength = temperature() < m_target_temperature;
    }

    every(5s) {
        LOG("resistance strength: ", strength);
    }

    control_resistance(strength);
}

void Boiler::turn_off_resistance() {
    update_target_temperature(0);
}

bool Boiler::is_alarm_triggered() const {
    return not digitalRead(Pin::WaterLevelAlarm);
}

void Boiler::reset() {
    m_last_checked_heating_temperature = 0.f;
    m_heating_check_timer.stop();

    m_outside_target_range_timer.stop();
}
}
