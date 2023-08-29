#include <lucas/cmd/cmd.h>
#include <lucas/Spout.h>
#include <lucas/RecipeQueue.h>
#include <src/gcode/parser.h>

namespace lucas::cmd {
void L2() {
    if (parser.seenval('D'))
        Spout::the().pour_with_digital_signal(parser.ulongval('T'), parser.ulongval('D'));
    else
        Spout::the().pour_with_desired_volume(parser.ulongval('T'), parser.floatval('G'));

    const bool associated_with_station = RecipeQueue::the().executing();
    util::idle_while([&] {
        // recipe foi cancelada por meios externos
        if (associated_with_station and not RecipeQueue::the().executing())
            return false;
        return Spout::the().pouring();
    });
}
}
