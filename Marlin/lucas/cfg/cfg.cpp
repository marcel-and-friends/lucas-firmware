#include <lucas/cfg/cfg.h>
#include <lucas/util/util.h>

namespace lucas::cfg {
constexpr auto OPCOES_DEFAULT = std::to_array<Opcao>({
    [LogDespejoBico] = {.id = 'D',  .ativo = true },
    [LogViagemBico] = { .id = 'V', .ativo = true },
    [LogFila] = { .id = 'F', .ativo = true },
    [LogNivelamento] = { .id = 'N', .ativo = true },
    [LogEstacoes] = { .id = 'E', .ativo = true },

    [LogSerial] = { .id = 'S', .ativo = false},
    [LogWifi] = { .id = 'W', .ativo = false},
    [LogGcode] = { .id = 'G', .ativo = false},

    [ModoGiga] = { .id = 'M', .ativo = false},

    [SetarTemperaturaTargetNoNivelamento] = { .id = 'T', .ativo = true },
    [AnalisarTabelaSalvaNoNivelamento] = { .id = 'A', .ativo = true },
    [PreencherTabelaDeFluxoNoNivelamento] = { .id = 'X', .ativo = true },
});

consteval bool nao_possui_opcoes_duplicadas(ListaOpcoes const& opcoes) {
    for (size_t i = 1; i < opcoes.size(); ++i)
        for (size_t j = 0; j < i; ++j)
            if (opcoes[i].id != Opcao::ID_DEFAULT and opcoes[j].id != Opcao::ID_DEFAULT)
                if (opcoes[i].id == opcoes[j].id)
                    return false;

    return true;
}

static_assert(OPCOES_DEFAULT.size() == Opcoes::Count, "tamanho errado irmao");
static_assert(nao_possui_opcoes_duplicadas(OPCOES_DEFAULT), "opcoes duplicadas irmao");

// vai ser inicializado na 'setup()'
static ListaOpcoes s_opcoes = {};

constexpr auto INICIO_FLASH = 400; // sizeof(Fila::ControladorFluxo::m_tabela)
constexpr auto OPCAO_SIZE = sizeof(char) + sizeof(bool);

static void ler_opcoes_da_flash() {
    util::fill_flash_buffer();

    for (size_t i = 0; i < s_opcoes.size(); ++i) {
        auto& opcao = s_opcoes[i];
        auto const offset = INICIO_FLASH + (i * OPCAO_SIZE);
        opcao.id = util::buffered_read_flash<char>(offset);
        opcao.ativo = util::buffered_read_flash<bool>(offset + sizeof(char));
    }

    LOG("opcoes lidas da flash");
}

void setup() {
    util::fill_flash_buffer();
    auto const primeiro_id = util::buffered_read_flash<char>(INICIO_FLASH);
    if (primeiro_id != OPCOES_DEFAULT[0].id) {
        LOG("cfg ainda nao foi salva na flash, usando valores padroes");
        s_opcoes = OPCOES_DEFAULT;
        salvar_opcoes_na_flash();
    } else {
        ler_opcoes_da_flash();
    }
}

void salvar_opcoes_na_flash() {
    for (size_t i = 0; i < s_opcoes.size(); ++i) {
        auto const& opcao = s_opcoes[i];
        auto const offset = INICIO_FLASH + (i * OPCAO_SIZE);
        util::buffered_write_flash<char>(offset, opcao.id);
        util::buffered_write_flash<bool>(offset + sizeof(char), opcao.ativo);
    }

    util::flush_flash();
    LOG("opcoes escritas na flash");
}

void resetar_opcoes() {
    s_opcoes = OPCOES_DEFAULT;
    for (size_t i = 0; i < s_opcoes.size(); ++i) {
        auto const offset = INICIO_FLASH + (i * OPCAO_SIZE);
        util::buffered_write_flash<char>(offset, Opcao::ID_DEFAULT);
        util::buffered_write_flash<bool>(offset + sizeof(char), false);
    }

    util::flush_flash();
}

Opcao get(Opcoes opcao) {
    return s_opcoes[size_t(opcao)];
}

ListaOpcoes& opcoes() {
    return s_opcoes;
}
}
