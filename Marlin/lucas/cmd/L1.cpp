#include <lucas/cmd/cmd.h>
#include <src/gcode/parser.h>
#include <lucas/Bico.h>
#include <lucas/Estacao.h>

namespace lucas::cmd {
void L1() {
    Bico::the().ativar(parser.ulongval('T'), parser.floatval('G'));
    while (Bico::the().ativo()) {
        idle();
    }
}
}
