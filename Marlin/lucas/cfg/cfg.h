#pragma once

#include <array>
#include <cstddef>

namespace lucas::cfg {
struct Opcao {
    static constexpr char ID_DEFAULT = 0x47;

    char id = ID_DEFAULT;
    bool ativo = true;
};

enum Opcoes {
    LogDespejoBico = 0,
    LogViagemBico,
    LogFila,
    LogNivelamento,
    LogEstacoes,

    LogSerial,
    LogWifi,
    LogGcode,

    ModoGiga,

    SetarTemperaturaTargetNoNivelamento,
    AnalisarTabelaSalvaNoNivelamento,
    PreencherTabelaDeFluxoNoNivelamento,

    Count
};

using ListaOpcoes = std::array<Opcao, size_t(Opcoes::Count)>;

void setup();

void salvar_opcoes_na_flash();

void resetar_opcoes();

Opcao get(Opcoes opcao);
ListaOpcoes& opcoes();

#define CFG(opcao) cfg::get(cfg::Opcoes::opcao).ativo
#define LOG_IF(opcao, ...)                                        \
    do {                                                          \
        if (CFG(opcao)) {                                         \
            const auto& opcao_cfg = cfg::get(cfg::Opcoes::opcao); \
            if (opcao_cfg.id != cfg::Opcao::ID_DEFAULT)           \
                SERIAL_ECHO(AS_CHAR(opcao_cfg.id));               \
            else                                                  \
                SERIAL_ECHO(AS_CHAR('?'));                        \
            LOG("", ": ", __VA_ARGS__);                           \
        }                                                         \
    } while (false)
}
