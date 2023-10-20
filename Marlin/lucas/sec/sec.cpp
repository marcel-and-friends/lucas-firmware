#include "sec.h"

#include <lucas/util/util.h>
#include <lucas/info/info.h>
#include <lucas/util/SD.h>
#include <lucas/Boiler.h>
#include <lucas/Spout.h>
#include <lucas/RecipeQueue.h>

namespace lucas::sec {
consteval auto make_default_return_conditions() {
    std::array<bool (*)(), usize(Error::Count)> conditions;
    // most of them are no return
    std::fill(
        conditions.begin(),
        conditions.end(),
        +[] {
            return false;
        });
    // except the water level one
    conditions[usize(Error::WaterLevelAlarm)] = +[] {
        return not Boiler::the().is_alarm_triggered();
    };
    return conditions;
}

constexpr auto SECURITY_FILE_PATH = "/sec.txt";
constexpr auto RETURN_CONDITIONS = make_default_return_conditions();

void setup() {
    auto sd = util::SD::make();
    if (sd.file_exists(SECURITY_FILE_PATH)) {
        if (not sd.open(SECURITY_FILE_PATH, util::SD::OpenMode::Read))
            return;

        Error reason_for_last_failure;
        sd.read_into(reason_for_last_failure);

        raise_error(reason_for_last_failure);
    }
}

static Error s_active_error = Error::Invalid;

void raise_error(Error reason) {
    // alarm was raised! oh no
    // inform the host that something has happened so that the user can be informed too
    s_active_error = reason;
    inform_active_error();

    // store the temperature we were at when the alarm was triggered
    // this way we can (potentially) go back to it
    const auto old_temperature = Boiler::the().target_temperature();
    // turn off the heaters, we don't want anything exploding in case there's no water on the boiler
    Boiler::the().turn_off_resistance();

    // disable the motor and pump
    if (Spout::the().pouring())
        Spout::the().end_pour();

    // cancel all recipes that are in queue
    if (not RecipeQueue::the().is_empty())
        RecipeQueue::the().cancel_all_recipes();

    auto sd = util::SD::make();
    if (sd.open(SECURITY_FILE_PATH, util::SD::OpenMode::Write))
        sd.write_from(reason);

    // Spout's 'tick()' isn't filtered so that the pump's BRK is properly released after 'end_pour()'
    constexpr auto FILTER = core::Filter::RecipeQueue | core::Filter::Boiler | core::Filter::Info;
    util::idle_until(RETURN_CONDITIONS[usize(reason)], FILTER);

    if (sd.is_open())
        sd.remove_file(SECURITY_FILE_PATH);

    // if this is reached then the security threat has been successfully dealt with
    // let the host know it is no longer active and go back to our old temperature
    s_active_error = Error::Invalid;
    inform_active_error();

    Boiler::the().set_target_temperature_and_wait(old_temperature);
}

bool has_active_error() {
    return s_active_error != Error::Invalid;
}

void inform_active_error() {
    info::send(
        info::Event::Security,
        [](JsonObject o) {
            o["reason"] = s32(s_active_error);
            o["active"] = has_active_error();
            if (s_active_error == Error::WaterLevelAlarm)
                o["currentTemp"] = Boiler::the().temperature();
        });
}

void remove_stored_error() {
    auto sd = util::SD::make();
    if (sd.file_exists(SECURITY_FILE_PATH))
        sd.remove_file(SECURITY_FILE_PATH);
}
}
