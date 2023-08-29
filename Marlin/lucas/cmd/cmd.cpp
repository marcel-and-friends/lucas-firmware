#include "cmd.h"
#include <cstring>
#include <src/gcode/parser.h>
#include <src/gcode/queue.h>
#include <src/gcode/gcode.h>
#include <lucas/Station.h>
#include <lucas/serial/Hook.h>

namespace lucas::cmd {
void execute(const char* gcode) {
    LOG_IF(LogGcode, "executando gcode: ", gcode);
    parser.parse(const_cast<char*>(gcode));
    GcodeSuite::process_parsed_command(true);
}

void interpret_gcode_from_host(std::span<char> buffer) {
    GcodeSuite::process_subcommands_now(buffer.data());
}
}

/* alguns comandos uteis
~ init ~
#{"cmdInitializeStations":[true, true, true, true, true]}#
#{"cmdSetBoilerTemperature":93}#
#{"cmdInitializeStations":[true, true, true, true, true],"cmdSetBoilerTemperature":93}#

~ recipes ~
#{"devScheduleStandardRecipe":1}#
#{"devSimulateButtonPress":0}#
#{"devScheduleStandardRecipe":1,"devSimulateButtonPress":0}#
#{"devScheduleStandardRecipe":5,"devSimulateButtonPress":[0,1,2,3,4]}#
#{"cmdCancelRecipe":0}#
#{"cmdCancelRecipe":[0,1,2,3,4]}#

~ rest ~
#{"reqInfoAllStations":[0,1,2,3,4]}#
*/
