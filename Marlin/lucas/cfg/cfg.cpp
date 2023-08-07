#include <lucas/cfg/cfg.h>
#include <lucas/Bico.h>
#include <lucas/mem/FlashReader.h>
#include <lucas/mem/FlashWriter.h>

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
    [LogLn] = { .id = 'L', .ativo = false},

    [ModoGiga] = { .id = 'M', .ativo = false},

    [SetarTemperaturaTargetNoNivelamento] = { .id = 'T', .ativo = true },
    [PreencherTabelaDeFluxoNoNivelamento] = { .id = 'X', .ativo = false},
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

// vai ser propriamente inicializado na 'setup()'
static ListaOpcoes s_opcoes = {};

constexpr auto INICIO_FLASH = sizeof(Bico::ControladorFluxo::Tabela);
constexpr auto OPCAO_SIZE = sizeof(char) + sizeof(bool);

void ler_opcoes_da_flash();

void setup() {
    auto reader = mem::FlashReader(INICIO_FLASH);
    auto const primeiro_id = reader.read<char>(INICIO_FLASH);
    if (primeiro_id == Opcao::ID_DEFAULT) {
        LOG("cfg ainda nao foi salva na flash, usando valores padroes");
        s_opcoes = OPCOES_DEFAULT;
        salvar_opcoes_na_flash();
    } else {
        ler_opcoes_da_flash();
    }
}

void salvar_opcoes_na_flash() {
    auto writer = mem::FlashWriter(INICIO_FLASH);
    for (size_t i = 0; i < s_opcoes.size(); ++i) {
        auto const& opcao = s_opcoes[i];
        auto const offset = i * OPCAO_SIZE;
        writer.write<char>(offset, opcao.id);
        writer.write<bool>(offset + sizeof(char), opcao.ativo);
    }

    LOG("opcoes escritas na flash");
}

void ler_opcoes_da_flash() {
    auto reader = mem::FlashReader(INICIO_FLASH);
    for (size_t i = 0; i < s_opcoes.size(); ++i) {
        auto& opcao = s_opcoes[i];
        auto const offset = i * OPCAO_SIZE;
        opcao.id = reader.read<char>(offset);
        opcao.ativo = reader.read<bool>(offset + sizeof(char));
    }

    LOG("opcoes lidas da flash");
}

void resetar_opcoes() {
    s_opcoes = OPCOES_DEFAULT;
    auto writer = mem::FlashWriter(INICIO_FLASH);
    for (size_t i = 0; i < s_opcoes.size(); ++i) {
        auto const offset = i * OPCAO_SIZE;
        writer.write<char>(offset, Opcao::ID_DEFAULT);
        writer.write<bool>(offset + sizeof(char), false);
    }
}

Opcao get(Opcoes opcao) {
    return s_opcoes[size_t(opcao)];
}

ListaOpcoes& opcoes() {
    return s_opcoes;
}
}
