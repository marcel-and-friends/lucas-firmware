#include <lucas/cmd/cmd.h>
#include <lucas/Bico.h>

namespace lucas::cmd {
void L6() {
    Bico::ControladorFluxo::the().limpar_tabela(Bico::ControladorFluxo::SalvarNaFlash::Sim);
    LOG("tabela foi limpa e escrita na flash");
}
}
