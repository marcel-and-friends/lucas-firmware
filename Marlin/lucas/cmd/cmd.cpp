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
#{"cmdSetBoilerTemperature":94}#
#{"cmdInitializeStations":[true, true, true, true, true],"cmdSetBoilerTemperature":94}#

~ recipes ~
#{"devScheduleStandardRecipe":1}#
#{"devSimulateButtonPress":0}#
#{"devSimulateButtonPress":[0,1,2,3,4]}#
#{"devScheduleStandardRecipe":1,"devSimulateButtonPress":0}#
#{"devScheduleStandardRecipe":5,"devSimulateButtonPress":[0,1,2,3,4]}#
#{"cmdCancelRecipe":0}#
#{"cmdCancelRecipe":[0,1,2,3,4]}#
#{"cmdSetFixedRecipes":{"recipes":[{"id":61680,"finalizationTime":60000,"scald":{"duration":6000,"gcode":"L0 D10.5 N3 R1 T6000 G80"},"attacks":[{"duration":6000,"gcode":"L0 D7 N3 R1 T6000 G60","interval":24000},{"duration":9000,"gcode":"L0 D7 N5 R1 T9000 G90","interval":30000},{"duration":10000,"gcode":"L0 D7 N5 R1 T10000 G100","interval":35000},{"duration":9000,"gcode":"L0 D7 N5 R1 T9000 G100"}]},{"id":61680,"finalizationTime":60000,"scald":{"duration":6000,"gcode":"L0 D10.5 N3 R1 T6000 G80"},"attacks":[{"duration":6000,"gcode":"L0 D7 N3 R1 T6000 G60","interval":24000},{"duration":9000,"gcode":"L0 D7 N5 R1 T9000 G90","interval":30000},{"duration":10000,"gcode":"L0 D7 N5 R1 T10000 G100","interval":35000},{"duration":9000,"gcode":"L0 D7 N5 R1 T9000 G100"}]},{"id":61680,"finalizationTime":60000,"scald":{"duration":6000,"gcode":"L0 D10.5 N3 R1 T6000 G80"},"attacks":[{"duration":6000,"gcode":"L0 D7 N3 R1 T6000 G60","interval":24000},{"duration":9000,"gcode":"L0 D7 N5 R1 T9000 G90","interval":30000},{"duration":10000,"gcode":"L0 D7 N5 R1 T10000 G100","interval":35000},{"duration":9000,"gcode":"L0 D7 N5 R1 T9000 G100"}]}]}}#

~ rest ~
#{"reqInfoAllStations":[0,1,2,3,4]}#
#{"devSimulateButtonPress":0}#
*/
