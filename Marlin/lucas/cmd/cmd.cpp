#include "cmd.h"
#include <cstring>
#include <src/gcode/parser.h>
#include <src/gcode/queue.h>
#include <src/gcode/gcode.h>
#include <lucas/Station.h>

namespace lucas::cmd {
void execute(const char* gcode) {
    LOG_IF(LogGcode, "executando gcode: ", gcode);
    parser.parse(const_cast<char*>(gcode));
    GcodeSuite::process_parsed_command(true);
}
}

/* alguns comandos uteis
#{"0":5,"1":93,"2":[true,true,true,true,true]}#
#{"7":1,"8":0}#
#{"3":0}#
#{"7":5,"8":[0,1,2,3,4]}#
#{"0":5,"1":20}#
*/
