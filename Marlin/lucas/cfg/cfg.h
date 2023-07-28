#pragma once

#include <array>

namespace lucas::cfg {
struct Opcao {
    char id = 0;
    bool ativo = true;
};

enum Opcoes {
    LogSerial = 0,
    LogDespejoBico,
    LogViagemBico,
    LogFila,
    LogNivelamento,
    LogWifi,
    LogGcode,
    LogEstacoes,
    ModoGiga,

    PreencherTabelaDeFluxoNoNivelamento,
    SetarTemperaturaTargetNoNivelamento
};

inline auto opcoes = std::to_array<Opcao>({
    [LogSerial] = { .id = 'S', .ativo = false },
    [LogDespejoBico] = { .id = 'D', .ativo = true },
    [LogViagemBico] = { .id = 'V', .ativo = true },
    [LogFila] = { .id = 'F', .ativo = true },
    [LogNivelamento] = { .id = 'N', .ativo = true },
    [LogWifi] = { .id = 'W', .ativo = false },
    [LogGcode] = { .id = 'G', .ativo = false },
    [LogEstacoes] = { .id = 'E', .ativo = true },
    [ModoGiga] = { .id = 'M', .ativo = false },

    [PreencherTabelaDeFluxoNoNivelamento] = { .ativo = true },
    [SetarTemperaturaTargetNoNivelamento] = { .ativo = true },
});
}

#define CFG(opcao) cfg::opcoes[cfg::Opcoes::opcao].ativo
#define LOG_IF(opcao, ...)                                                \
    do {                                                                  \
        if (CFG(opcao)) {                                                 \
            if (cfg::opcoes[cfg::Opcoes::opcao].id)                       \
                SERIAL_ECHO(AS_CHAR(cfg::opcoes[cfg::Opcoes::opcao].id)); \
            else                                                          \
                SERIAL_ECHO(AS_CHAR('?'));                                \
            LOG("", ": ", __VA_ARGS__);                                   \
        }                                                                 \
    } while (false)
