#include "cmd.h"
#include <cstring>
#include <src/gcode/parser.h>
#include <src/gcode/queue.h>
#include <src/gcode/gcode.h>
#include <lucas/Estacao.h>

namespace lucas::cmd {
void executar(const char* gcode) {
    if (CFG(LogGcode))
        LOG("executando gcode: ", gcode);

    parser.parse(const_cast<char*>(gcode));
    GcodeSuite::process_parsed_command(true);
}
}
