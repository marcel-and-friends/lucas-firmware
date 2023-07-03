#include <lucas/cmd/cmd.h>
#include <lucas/Bico.h>

namespace lucas::cmd {
void L6() {
    Bico::the().preencher_tabela_de_controle_de_fluxo();
}
}
