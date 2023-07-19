#pragma once

#include <array>

namespace lucas::cfg {
struct Opcao {
    char id = 0;
    bool ativo = false;
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

    ConectarWifiAuto,
    PreencherTabelaDeFluxoNoNivelamento
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

    [ConectarWifiAuto] = { .ativo = false },
    [PreencherTabelaDeFluxoNoNivelamento] = { .ativo = false },
});
}

#define CFG(opcao) cfg::opcoes[cfg::Opcoes::opcao].ativo
#define LOG_IF(opcao, ...)                                            \
    do {                                                              \
        if (CFG(opcao)) {                                             \
            SERIAL_ECHO(AS_CHAR(cfg::opcoes[cfg::Opcoes::opcao].id)); \
            LOG("", ": ", __VA_ARGS__);                               \
        }                                                             \
    } while (false)
