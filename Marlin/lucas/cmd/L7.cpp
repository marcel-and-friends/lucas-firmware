#include <lucas/cmd/cmd.h>
#include <lucas/storage/storage.h>
#include <src/gcode/parser.h>

namespace lucas::cmd {
void L7() {
    if (parser.seenval('E'))
        storage::purge_entry(parser.value_int());
}
}
