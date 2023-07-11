#include <lucas/cmd/cmd.h>
#include <lucas/Fila.h>
#include <lucas/Receita.h>
#include <lucas/cfg/cfg.h>

namespace lucas::cmd {
void L7() {
    for (auto& opcao : cfg::opcoes)
        if (parser.seenval(opcao.letra))
            opcao.ativo = !opcao.ativo;

    if (parser.seenval('Z')) {
        const auto pino = parser.value_int();
        SET_INPUT(pino);
        LOG("PINO = ", analogRead(pino));
    }
}
}
