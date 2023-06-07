#include "Receita.h"
#include <lucas/gcode/gcode.h>

namespace lucas {
const Receita& Receita::padrao() {
    constexpr std::string_view gcode = R"(L1 G1560 T6000
L3 F10250 D9 N3 R1
L0
L1 G1560 T6000
L3 F8250 D7 N3 R1
L2 T24000
L1 G1560 T9000
L3 F8500 D7 N5 R1
L2 T30000
L1 G1560 T10000
L3 F7650 D7 N5 R1
L2 T35000
L1 G1560 T10000
L3 F7650 D7 N5 R1)";
    static Receita padrao(gcode, 0xF0F0);
    return padrao;
}

Receita::Receita(const char* gcode, size_t id)
    : m_gcode_offset{ 0 }
    , m_id{ id } {
    const auto len = strlen(gcode);
    memcpy(m_gcode, gcode, len);
    m_gcode[len] = '\0';
}

Receita::Receita(std::string_view gcode, size_t id)
    : m_gcode_offset{ 0 }
    , m_id{ id } {
    memcpy(m_gcode, gcode.data(), gcode.size());
    m_gcode[gcode.size()] = '\0';
}

std::string_view Receita::proxima_instrucao() const {
    return gcode::proxima_instrucao(m_gcode + m_gcode_offset);
}

void Receita::reiniciar() {
    memset(m_gcode, 0, sizeof(m_gcode));
    m_gcode_offset = 0;
    m_id = 0;
}

void Receita::prosseguir() {
    auto instrucao = proxima_instrucao();
    gcode::injetar(instrucao.data());
    m_gcode_offset += instrucao.size() + 1;
}

bool Receita::vazia() const {
    return m_gcode[0] == '\0';
}

}
