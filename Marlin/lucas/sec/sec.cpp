#include "sec.h"

#include <lucas/util/util.h>
#include <lucas/info/info.h>
#include <lucas/Boiler.h>
#include <lucas/Spout.h>
#include <lucas/RecipeQueue.h>

namespace lucas::sec {
using ValidityList = std::array<bool, size_t(Reason::Count)>;
namespace detail {
constexpr auto default_validity() {
    ValidityList result;
    // all reasons are valid by default
    std::fill(result.begin(), result.end(), true);
    return result;
}
}

static ValidityList s_validity = detail::default_validity();

void raise_error(Reason reason, IdleWhile condition) {
    if (s_validity[size_t(reason)] == false) {
        LOG_ERR("reason is not valid for raising");
        return;
    }

    info::send(info::Event::Security, [reason](JsonObject o) {
        o["reason"] = int(reason);
        o["active"] = true;
    });
    const auto old_temperature = Boiler::the().target_temperature();
    Boiler::the().disable_heater();

    if (Spout::the().pouring())
        Spout::the().end_pour();

    if (not RecipeQueue::the().is_empty())
        RecipeQueue::the().cancel_all_recipes();

    // spout's 'tick()' isn't filtered so that the pump's BRK is properly released after 'end_pour()'
    constexpr auto FILTER = TickFilter::All & ~TickFilter::Spout;
    util::idle_while(
        condition ? condition : [] { return true; }, // if no condition is provided, idle forever
        FILTER);

    info::send(info::Event::Security, [reason](JsonObject o) {
        o["reason"] = int(reason);
        o["active"] = false;
    });
    Boiler::the().set_target_temperature_and_wait(old_temperature);
}

bool is_reason_valid(Reason reason) {
    return s_validity[size_t(reason)];
}

void toggle_reason_validity(Reason reason) {
    s_validity[size_t(reason)] = not s_validity[size_t(reason)];
}
}
