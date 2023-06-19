#include "Bico.h"
#include <lucas/lucas.h>
#include <lucas/gcode/gcode.h>
#include <src/module/temperature.h>
#include <src/module/planner.h>
#include <cmath>

namespace lucas {

#define BICO_LOG(...) LOG("", "BICO: ", __VA_ARGS__)

namespace pino {
constexpr auto ENABLE = PA3;
constexpr auto SV = PA5;
constexpr auto BREAK = PB2;
constexpr auto REF = PE6;
constexpr auto FLUXO = 0x47;
};

constexpr auto SINAL_MAX = 4095;
constexpr auto SINAL_MIN = 0;

struct Input {
    millis_t tempo;
    float gramas;

    constexpr float fluxo_ideal() const {
        return tempo / 1000 / gramas;
    }
};

namespace util {
inline float fluxo_atual() {
    // valores medidos com uma balança
    constexpr auto fluxo_min = 0.f;   // 0 volts - 0 digital
    constexpr auto fluxo_max = 16.6f; // 5 volts - 255 digital
    const auto leitura = static_cast<float>(analogRead(pino::FLUXO));
    return std::lerp(fluxo_min, fluxo_max, leitura / 255.f);
}

constexpr auto TEMPO_PARA_DESLIGAR_O_BREAK = 2000; // 2s
}

void Bico::tick(millis_t tick) {
    if (!m_ativo) {
        if (m_tick_final) {
            // após desligar o motor deixamos o break ativo por um tempinho e depois liberamos
            if (tick - m_tick_final >= util::TEMPO_PARA_DESLIGAR_O_BREAK) {
                digitalWrite(pino::BREAK, HIGH);
                m_tick_final = 0;
            }
        }
        return;
    }

    if (tick - m_tick_comeco >= m_duracao) {
        BICO_LOG("finalizando despejo [delta: ", tick - m_tick_comeco, "]");
        desligar(tick);
    }
}

void Bico::ativar(millis_t tick, millis_t tempo, float gramas) {
    if (!tempo || !gramas)
        return;

    const auto gramas_para_forca_3_grau = [](float gramas) {
        return -0.0071f * powf(gramas, 3.f) + 0.7413f * powf(gramas, 2.f) + 113.35f * gramas - 464.36f;
    };

    const auto forca = gramas_para_forca_3_grau(gramas);
    const auto valor_digital = static_cast<int>(std::roundf(forca));
    const auto valor_final = static_cast<uint32_t>(std::clamp(valor_digital, 0, 4095));
    const auto valor_final_owo = static_cast<uint32_t>(std::clamp(gramas, 0.f, 4095.f));

    m_ativo = true;
    m_forca = valor_final_owo;
    m_tick_comeco = tick;
    m_duracao = tempo;

    digitalWrite(pino::BREAK, HIGH); // libera o motor
    analogWrite(pino::SV, m_forca);  // aplica a força

    BICO_LOG("iniciando despejo [T: ", m_duracao, " - P: ", m_forca, " - tick: ", m_tick_comeco, "]");
}

void Bico::desligar(millis_t tick_desligou) {
    m_ativo = false;
    m_forca = 0;
    m_tick_comeco = 0;
    m_duracao = 0;
    m_tick_final = tick_desligou;

    digitalWrite(pino::BREAK, LOW); // breca o motor
    analogWrite(pino::SV, LOW);     // zera a força
}

void Bico::setup() {
    // nossos pinos DAC tem resolução de 12 bits
    analogWriteResolution(12);

    // os 4 porquinhos
    pinMode(pino::SV, OUTPUT);
    pinMode(pino::ENABLE, OUTPUT);
    pinMode(pino::BREAK, OUTPUT);
    pinMode(pino::REF, OUTPUT);

    // o pino de referencia fica permamentemente com 3v, pro conversor de sinal funcionar direito
    digitalWrite(pino::REF, HIGH);
    // e o enable permamentemente ligado, o motor é controlado somente pelo break e sv
    digitalWrite(pino::ENABLE, LOW);

    // bico começa desligado
    desligar(millis());

    // rotina de nivelamento roda quando a placa liga
    nivelar();
}

void Bico::viajar_para_estacao(Estacao& estacao) const {
    viajar_para_estacao(estacao.index());
}

void Bico::viajar_para_estacao(Estacao::Index index) const {
    BICO_LOG("indo para a estacao #", index + 1);
    auto comeco = millis();
    auto movimento = util::ff("G0 F50000 Y60 X%s", Estacao::posicao_absoluta(index));
    cmd::executar_cmds("G90",
                       movimento,
                       "G91",
                       "M400");
    BICO_LOG("demorou ", millis() - comeco, "ms");
}

void Bico::viajar_para_esgoto() const {
    BICO_LOG("indo para o esgoto");
    cmd::executar_cmds("G90",
                       "G0 F50000 Y60 X5",
                       "G91",
                       "M400");
}

bool Bico::esta_na_estacao(Estacao::Index index) const {
    return planner.get_axis_position_mm(X_AXIS) == Estacao::posicao_absoluta(index);
}

void Bico::descartar_agua_ruim() const {
    constexpr auto TEMP_IDEAL = 70.f;
    constexpr auto TEMPO_DESCARTE = 1000;

    if (thermalManager.degHotend(0) >= TEMP_IDEAL)
        return;

    viajar_para_esgoto();

    auto tick = millis() - TEMPO_DESCARTE;
    while (true) {
        if (millis() - tick >= TEMPO_DESCARTE) {
            tick = millis();
            if (thermalManager.degHotend(0) < TEMP_IDEAL) {
                LOG("jogando agua");
                cmd::executar_fmt("L1 G1300 T%d", TEMPO_DESCARTE);
            } else {
                break;
            }
        } else {
            idle();
        }
    }
}

void Bico::nivelar() const {
    BICO_LOG("executando rotina de nivelamento");
    cmd::executar("G28 XY");
#if LUCAS_ROTINA_TEMP
    cmd::executar_ff("M140 S%s", 93.f);
#endif
    BICO_LOG("nivelamento finalizado");
}
}
