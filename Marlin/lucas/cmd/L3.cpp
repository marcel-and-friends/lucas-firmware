#include <lucas/cmd/cmd.h>
#include <lucas/Station.h>
#include <lucas/MotionController.h>
#include <src/gcode/parser.h>
#include <algorithm>

namespace lucas::cmd {
void L3() {
    constexpr auto max = static_cast<long>(Station::MAXIMUM_NUMBER_OF_STATIONS);
    auto num = parser.longval('N');
    if (num == -1)
        MotionController::the().travel_to_sewer();
    else
        MotionController::the().travel_to_station(std::clamp(num, 0L, max), parser.floatval('O'));
}
}
