#include "sec.h"
#include <lucas/util/util.h>
#include <lucas/info/info.h>
#include <lucas/storage/storage.h>
#include <lucas/core/core.h>
#include <lucas/Boiler.h>
#include <lucas/Spout.h>
#include <lucas/RecipeQueue.h>
#include <utility>

namespace lucas::sec {
namespace detail {
using ConditionList = std::array<bool (*)(), usize(Error::Count)>;
consteval ConditionList make_default_return_conditions() {
    ConditionList result;
    // most of them are no return
    std::fill(
        result.begin(),
        result.end(),
        +[] { return false; });

    // except the water level one
    result[usize(Error::WaterLevelAlarm)] = +[] {
        return not Boiler::the().is_alarm_triggered();
    };

    return result;
}
}

constexpr auto RETURN_CONDITIONS = detail::make_default_return_conditions();

static auto s_active_error = Error::Invalid;
static auto s_startup_error = Error::Invalid;
static storage::Handle s_storage_handle;

void setup() {
    s_storage_handle = storage::register_handle_for_entry("error", sizeof(Error));

    auto entry = storage::fetch_entry(s_storage_handle);
    if (not entry)
        return;

    s_startup_error = entry->read_binary<Error>();
}

void tick() {
    if (s_startup_error != Error::Invalid)
        raise_error(std::exchange(s_startup_error, Error::Invalid));
}

static void inform_active_error() {
    info::send(
        info::Event::Security,
        [](JsonObject o) {
            o["reason"] = s32(s_active_error);
            o["active"] = s_active_error != Error::Invalid;
            if (s_active_error == Error::WaterLevelAlarm)
                o["currentTemp"] = Boiler::the().temperature();
        });
}

static void update_and_inform_active_error(Error error) {
    s_active_error = error;
    inform_active_error();
}

void raise_error(Error reason) {
    // inform the host that something has happened so that the user can be informed too
    update_and_inform_active_error(reason);

    // store the temperature we were at when the alarm was triggered
    // this way we can (potentially) go back to it
    const auto old_temperature = Boiler::the().target_temperature();
    // turn off the heaters, we don't want anything exploding in case there's no water on the boiler
    Boiler::the().turn_off_resistance();

    // disable the motor and pump
    Spout::the().end_pour();

    // cancel all recipes that are in queue
    RecipeQueue::the().cancel_all_recipes();

    // the water level alarm is a special case since, at startup, it is dealt with by the boiler
    if (reason != Error::WaterLevelAlarm) {
        // save the reason for when we next start up
        auto entry = storage::fetch_or_create_entry(s_storage_handle);
        entry.write_binary(reason);
    }

    // free the motor so we don't put unnecessary pressure on it
    digitalWrite(Spout::BRK, HIGH);

    {
        info::TemporaryCommandHook hook{
            info::Command::RequestInfoCalibration,
            &inform_active_error
        };

        util::maintenance_idle_until(RETURN_CONDITIONS[usize(reason)]);
    }

    storage::purge_entry(s_storage_handle);

    // if we reach this point that means the error has been dealt with somehow

    update_and_inform_active_error(Error::Invalid);

    // go back to our old temperature
    Boiler::the().update_target_temperature(old_temperature);

    core::inform_calibration_status();
}
}
