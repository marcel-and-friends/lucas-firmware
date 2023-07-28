#include <lucas/cfg/cfg.h>
#include <lucas/util/util.h>

constexpr auto INICIO_FLASH = 400;

namespace lucas::cfg {
constexpr auto OPCOES_DEFAULT = std::to_array<Opcao>({
    [LogSerial] = { .id = 'S', .ativo = false },
    [LogDespejoBico] = { .id = 'D', .ativo = true },
    [LogViagemBico] = { .id = 'V', .ativo = true },
    [LogFila] = { .id = 'F', .ativo = true },
    [LogNivelamento] = { .id = 'N', .ativo = true },
    [LogTabelaNivelamento] = { .id = 'T', .ativo = false },
    [LogWifi] = { .id = 'W', .ativo = false },
    [LogGcode] = { .id = 'G', .ativo = false },
    [LogEstacoes] = { .id = 'E', .ativo = true },
    [ModoGiga] = { .id = 'M', .ativo = false },

    [SetarTemperaturaTargetInicial] = { .ativo = false },
    [PreencherTabelaDeFluxoNoNivelamento] = { .ativo = true }
});

consteval bool nao_possui_opcoes_duplicadas(ListaOpcoes const& opcoes) {
    for (size_t i = 1; i < opcoes.size(); ++i)
        for (size_t j = 0; j < i; ++j)
            if (opcoes[i].id != Opcao::ID_DEFAULT and opcoes[j].id != Opcao::ID_DEFAULT)
                if (opcoes[i].id == opcoes[j].id)
                    return false;

    return true;
}

static_assert(OPCOES_DEFAULT.size() == Opcoes::__Count, "tamanho errado irmao");
static_assert(nao_possui_opcoes_duplicadas(OPCOES_DEFAULT), "opcoes duplicadas irmao");

// vai ser inicializado na 'setup()'
static ListaOpcoes s_opcoes = {};

static void escrever_opcoes_na_flash() {
    constexpr auto SIZE = sizeof(char) + sizeof(bool);
    for (size_t i = 0; i < s_opcoes.size(); ++i) {
        auto const& opcao = s_opcoes[i];
        util::buffered_write_flash<char>(INICIO_FLASH + (i * SIZE), opcao.id);
        util::buffered_write_flash<bool>(INICIO_FLASH + (i * SIZE) + sizeof(char), opcao.ativo);
    }

    util::flush_flash();
}

void setup() {
    util::fill_buffered_flash();
    auto const valor_inicio = util::buffered_read_flash<char>(INICIO_FLASH);

    if (valor_inicio != OPCOES_DEFAULT[0].id or valor_inicio == Opcao::ID_DEFAULT) {
        s_opcoes = OPCOES_DEFAULT;
        escrever_opcoes_na_flash();
        return;
    }
}

Opcao const& get(Opcoes opcao) {
    return s_opcoes[size_t(opcao)];
}

ListaOpcoes& opcoes() {
    return s_opcoes;
}

}
