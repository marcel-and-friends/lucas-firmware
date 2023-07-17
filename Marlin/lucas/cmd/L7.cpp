#include <lucas/cmd/cmd.h>
#include <lucas/Fila.h>
#include <lucas/Receita.h>
#include <lucas/cfg/cfg.h>

namespace lucas::cmd {
void L7() {
    for (auto& opcao : cfg::opcoes) {
        if (opcao.id && parser.seen(opcao.id)) {
            opcao.ativo = !opcao.ativo;
            LOG("opcao \'", AS_CHAR(opcao.id), "\' foi ", !opcao.ativo ? "des" : "", "ativada");
        }
    }

    if (parser.seenval('Z')) {
        const auto pino = parser.value_int();
        SET_INPUT(pino);
        LOG("PINO = ", analogRead(pino));
    }
}
}
