#pragma once

#include <array>

namespace lucas::cfg {
struct Opcao {
    char letra;
    bool ativo;
};

enum Opcoes {
    LogBufferSerial = 0,
    LogDespejoBico,
    LogViagemBico,
    LogFila,
    LogNivelamento,
    LogWifi,
};

inline auto opcoes = std::to_array<Opcao>({
    [LogBufferSerial] = {'S',  false},
    [LogDespejoBico] = { 'D', true },
    [LogViagemBico] = { 'V', true },
    [LogFila] = { 'F', true },
    [LogNivelamento] = { 'N', true },
    [LogWifi] = { 'W', true },
});
}

#define CFG(opcao) cfg::opcoes[cfg::Opcoes::opcao].ativo
#define LOG_IF(opcao, ...)                                               \
    do {                                                                 \
        if (CFG(opcao)) {                                                \
            SERIAL_ECHO(AS_CHAR(cfg::opcoes[cfg::Opcoes::opcao].letra)); \
            LOG("", ": ", __VA_ARGS__);                                  \
        }                                                                \
    } while (false)
