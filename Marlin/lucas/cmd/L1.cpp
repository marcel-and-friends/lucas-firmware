#include <lucas/cmd/cmd.h>
#include <src/gcode/parser.h>
#include <lucas/Bico.h>
#include <lucas/Estacao.h>

namespace lucas::cmd {
void L1() {
    if (parser.seenval('D'))
        Bico::the().desepejar_com_valor_digital(parser.ulongval('T'), parser.ulongval('D'));
    else
        Bico::the().despejar_volume(parser.ulongval('T'), parser.floatval('G'));

    while (Bico::the().ativo())
        idle();
}
}
