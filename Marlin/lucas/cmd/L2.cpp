#include <lucas/cmd/cmd.h>
#include <src/gcode/parser.h>
#include <lucas/Spout.h>
#include <lucas/RecipeQueue.h>
#include <lucas/Station.h>

namespace lucas::cmd {
void L2() {
    if (parser.seenval('D'))
        Spout::the().pour_with_digital_signal(parser.ulongval('T'), parser.ulongval('D'));
    else
        Spout::the().pour_with_desired_volume(parser.ulongval('T'), parser.floatval('G'), Spout::CorrectFlow::Yes);

    bool const associated_with_station = RecipeQueue::the().executing();
    util::wait_while([&] {
        // recipe foi cancelada por meios externos
        if (associated_with_station and not RecipeQueue::the().executing())
            return false;
        return Spout::the().pouring();
    });
}
}
