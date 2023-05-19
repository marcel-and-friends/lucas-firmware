#include "Bico.h"
#include <lucas/lucas.h>
#include <lucas/Estacao.h>
#include <src/module/temperature.h>
#include <cmath>

namespace lucas {
constexpr auto PINO_SV = MT_DET_1_PIN;
constexpr auto PINO_ENABLE = MT_DET_2_PIN;
constexpr auto PINO_BREAK = PB2;

void Bico::tick(millis_t tick) {
    if (!m_ativo) {
        if (m_tick_desligou) {
            if (tick - m_tick_desligou > 10000) {
                digitalWrite(PINO_BREAK, LOW);
                m_tick_desligou = 0;
            }
        }
        return;
    }

    if (tick - m_tick > m_tempo) {
        LOG("bico - finalizando [T: ", m_tempo, " - P: ", m_poder, " - tick: ", m_tick, " - diff: ", tick - m_tick, "]");
        m_tick_desligou = tick;
        reset();
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

    LOG("gramas = ", gramas, " | forca = ", forca);
    m_poder = valor_final;
    m_ativo = true;
    m_tick = tick;
    m_tempo = tempo;
    LOG("bico - iniciando [T: ", m_tempo, " - P: ", m_poder, " - tick: ", m_tick, "]");

    analogWrite(PINO_ENABLE, 4095); // libera a comunicação
    analogWrite(PINO_SV, m_poder);  // seta a força desejada
    digitalWrite(PINO_BREAK, 0);    // libera o break
}

void Bico::reset() {
    m_ativo = false;
    m_poder = 0;
    m_tick = 0;
    m_tempo = 0;

    digitalWrite(PINO_BREAK, HIGH); // breca o motor
    analogWrite(PINO_SV, 0);        // zera a força
    analogWrite(PINO_ENABLE, 0);    // desliga comunicação
}

void Bico::setup() {
    // nossos pinos DAC tem resolução de 12 bits
    analogWriteResolution(12);
    // os 3 porquinhos
    pinMode(PINO_SV, OUTPUT);
    pinMode(PINO_ENABLE, OUTPUT);
    pinMode(PINO_BREAK, OUTPUT);

    digitalWrite(PINO_BREAK, LOW);
    analogWrite(PINO_SV, 0);
    analogWrite(PINO_ENABLE, 0);

    nivelar();
}

void Bico::viajar_para_estacao(Estacao& estacao) const {
    viajar_para_estacao(estacao.numero());
}

void Bico::viajar_para_estacao(size_t numero) const {
    LOG("bico - indo para estacao #", numero);
    auto& estacao = Estacao::lista().at(numero - 1);
    gcode::executar("G90");
    gcode::executarff("G0 F50000 Y60 X%s", estacao.posicao_absoluta());
    gcode::executar("G91");
}

void Bico::descartar_agua_ruim() const {
#if LUCAS_ROTINA_TEMP
    constexpr auto TEMP_IDEAL = 90.f;
    constexpr auto MARGEM_ERRO_TEMP = 10.f;

    constexpr auto DESCARTE =
        R"(G0 F50000 Y60 X10
L1 P80 T10000
M109 T0 R%s)";
    // se a temperatura não é ideal (dentro da margem de erro) nós temos que regulariza-la antes de começarmos a receita
    if (fabs(thermalManager.degHotend(0) - TEMP_IDEAL) >= MARGEM_ERRO_TEMP)
        gcode::executarff(DESCARTE, TEMP_IDEAL);
#endif
}

void Bico::nivelar() const {
    LOG("bico - executando rotina de nivelamento");
    gcode::executar("G28 XY");
#if LUCAS_ROTINA_TEMP
    // gcode::executarff("M190 S%s", 30.f);
#endif
    LOG("bico - nivelamento finalizado");
}

}
