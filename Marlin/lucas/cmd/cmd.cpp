#include "cmd.h"
#include <cstring>
#include <src/gcode/parser.h>
#include <src/gcode/queue.h>
#include <src/gcode/gcode.h>
#include <lucas/Station.h>

namespace lucas::cmd {
void execute(char const* gcode) {
    LOG_IF(LogGcode, "executando gcode: ", gcode);
    parser.parse(const_cast<char*>(gcode));
    GcodeSuite::process_parsed_command(true);
}
}
