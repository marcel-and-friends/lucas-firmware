#include <lucas/Bico.h>
#include <src/gcode/parser.h>

namespace lucas::gcode {
void L5() {
    if (parser.seenval('A'))
        Bico::set_min(parser.value_long());

    if (parser.seenval('B'))
        Bico::set_max(parser.value_long());
}
}
