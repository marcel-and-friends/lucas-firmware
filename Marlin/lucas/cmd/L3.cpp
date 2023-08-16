#include <lucas/cmd/cmd.h>
#include <src/gcode/parser.h>
#include <lucas/Station.h>
#include <lucas/Spout.h>
#include <algorithm>

namespace lucas::cmd {
void L3() {
    constexpr auto max = static_cast<long>(Station::MAXIMUM_STATIONS);
    auto num = parser.longval('N');
    if (num == -1)
        Spout::the().travel_to_sewer();
    else
        Spout::the().travel_to_station(std::clamp(num, 0L, max), parser.floatval('O'));
}
}
