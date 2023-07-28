#include <lucas/cmd/cmd.h>
#include <lucas/Fila.h>
#include <lucas/Bico.h>
#include <lucas/Receita.h>
#include <lucas/cfg/cfg.h>

namespace lucas::cmd {
void L4() {
    bool atualizou = false;
    for (auto& opcao : cfg::opcoes()) {
        if (opcao.id != cfg::Opcao::ID_DEFAULT and parser.seen(opcao.id)) {
            opcao.ativo = not opcao.ativo;
            LOG("opcao \'", AS_CHAR(opcao.id), "\' foi ", not opcao.ativo ? "des" : "", "ativada");
            atualizou = true;
        }
    }

    if (atualizou)
        cfg::salvar_opcoes_na_flash();

    if (parser.seen('Y')) {
        cfg::resetar_opcoes();
        LOG("opcoes foram resetadas e salvas na flash");
    }

    if (parser.seen('Z')) {
        Bico::ControladorFluxo::the().limpar_tabela(Bico::ControladorFluxo::SalvarNaFlash::Sim);
        LOG("tabela foi limpa e salva na flash");
    }
}
}
