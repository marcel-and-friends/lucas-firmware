#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace lucas {
class Receita {
public:
    static const Receita& padrao();

    Receita() = default;

    Receita(const char* gcode, size_t id);

    Receita(std::string_view gcode, size_t id);

    std::string_view proxima_instrucao() const;

    void reiniciar();

    void prosseguir();

    bool vazia() const;

    size_t id() const {
        return m_id;
    }

private:
    char m_gcode[512] = {};
    ptrdiff_t m_gcode_offset = 0;
    size_t m_id = 0;
};
}
