#include "Bico.h"
#include <lucas/lucas.h>
#include <lucas/cmd/cmd.h>
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
constexpr auto FLUXO = PA13;
};

void Bico::tick() {
    const auto tick = millis();
    if (m_ativo) {
        m_tempo_decorrido = tick - m_tick_comeco;
        if (m_tempo_decorrido >= m_duracao) {
            desligar(tick);
            return;
        }
        if (m_tempo_decorrido > 1000) {
            if (!m_pulsos_no_inicio_da_analise)
                m_pulsos_no_inicio_da_analise = s_contador_de_pulsos;

            if ((m_tempo_decorrido % 1000) == 0) {
                const auto pulsos = s_contador_de_pulsos - m_pulsos_no_inicio_da_analise;
                m_pulsos_no_inicio_da_analise = s_contador_de_pulsos;
                m_forca = ControladorFluxo::the().analisar_e_melhorar_fluxo(m_forca, m_fluxo_desejado, pulsos);
                aplicar_forca(m_forca);
            }
        }
    } else {
        // após desligar o motor deixamos o break ativo por um tempinho e depois liberamos
        constexpr auto TEMPO_PARA_DESLIGAR_O_BREAK = 2000; // 2s
        if (m_tick_final) {
            if (tick - m_tick_final >= TEMPO_PARA_DESLIGAR_O_BREAK) {
                digitalWrite(pino::BREAK, HIGH);
                m_tick_final = 0;
            }
        }
    }
}

void Bico::ativar(millis_t tempo, float fluxo_desejado) {
    if (!tempo || !fluxo_desejado)
        return;

    m_ativo = true;
    m_tick_comeco = millis();
    m_duracao = tempo;
    m_fluxo_desejado = fluxo_desejado;
    m_forca = ControladorFluxo::the().melhor_valor_digital(fluxo_desejado);

    aplicar_forca(m_forca);
}

void Bico::desligar(millis_t tick_final) {
    m_ativo = false;
    m_forca = 0;
    m_tick_final = tick_final;

    m_duracao = 0;
    m_tempo_decorrido = 0;
    m_pulsos_no_inicio_da_analise = 0;

    aplicar_forca(m_forca);
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

    // rotina de nivelamento roda quando a placa liga
    nivelar();

    pinMode(pino::FLUXO, INPUT);

    attachInterrupt(
        digitalPinToInterrupt(pino::FLUXO),
        +[] {
            ++s_contador_de_pulsos;
        },
        RISING);

    ControladorFluxo::the().montar_tabela();
}

void Bico::viajar_para_estacao(Estacao& estacao) const {
    viajar_para_estacao(estacao.index());
}

void Bico::viajar_para_estacao(Estacao::Index index) const {
    BICO_LOG("viajando para a estacao #", index);
    const auto comeco = millis();
    const auto movimento = util::ff("G0 F50000 Y60 X%s", Estacao::posicao_absoluta(index));
    cmd::executar_cmds("G90",
                       movimento,
                       "G91",
                       "M400");
    BICO_LOG("chegou em ", millis() - comeco, "ms");
}

void Bico::viajar_para_esgoto() const {
    BICO_LOG("indo para o esgoto");
    const auto comeco = millis();
    cmd::executar_cmds("G90",
                       "G0 F50000 Y60 X5",
                       "G91",
                       "M400");
    BICO_LOG("chegou em ", millis() - comeco, "ms");
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

void Bico::aplicar_forca(uint32_t v) {
    m_forca = v;
    digitalWrite(pino::BREAK, !!v);
    analogWrite(pino::SV, m_forca);
}

void Bico::ControladorFluxo::montar_tabela() {
    constexpr auto VALOR_DIGITAL_INICIAL = 900;
    constexpr auto INCREMENTO_INICIAL = 100;
    constexpr auto TEMPO_DE_ESTABILIZAR_FLUXO = 2000;

    Bico::the().viajar_para_esgoto();

    for (auto& valores_digitais : tabela)
        for (auto& valor : valores_digitais)
            valor = VALOR_DIGITAL_INVALIDO;

    static int ultimo_valor_digital = 0;
    static float ultimo_volume_despejado = 0;

    int incremento = INCREMENTO_INICIAL;
    int valor_digital = VALOR_DIGITAL_INICIAL;
    while (true) {
        LOG("aplicando força de ", valor_digital);

        Bico::the().aplicar_forca(valor_digital);
        util::aguardar_por(TEMPO_DE_ESTABILIZAR_FLUXO);
        LOG("fluxo estabilizou, vamos analisar");

        const auto contador_comeco = s_contador_de_pulsos;
        util::aguardar_por(1000);
        const auto pulsos = static_cast<uint32_t>(s_contador_de_pulsos - contador_comeco);

        const auto volume_despejado = static_cast<float>(pulsos) * ML_POR_PULSO;

        if (ultimo_volume_despejado) {
            const auto delta = volume_despejado - ultimo_volume_despejado;
            if (delta > 1.f) {
                const auto delta_valor_digital = std::abs(valor_digital - ultimo_valor_digital);
                ultimo_valor_digital = valor_digital;
                incremento = std::max(delta_valor_digital / 2, 1);
                valor_digital -= incremento;
                LOG("pulo muito grande de força! - volume_despejado = ", volume_despejado, " | delta = ", delta, " | incremento = ", incremento);
                continue;
            }
        }

        if (volume_despejado < FLUXO_MIN) {
            valor_digital += incremento;
            LOG("ainda nao temos fluxo o suficiente");
            continue;
        }
        if (volume_despejado > FLUXO_MAX) {
            LOG("fluxo demais");
            break;
        }

        ultimo_volume_despejado = volume_despejado;
        ultimo_valor_digital = valor_digital;

        const auto volume_inteiro = static_cast<int>(volume_despejado);
        const auto casa_decimal = casa_decimal_apropriada(volume_despejado);

        LOG("volume_despejado = ", volume_despejado, " | casa_decimal = ", casa_decimal);

        auto& valor_digital_salvo = valor_digital_para_fluxo(volume_despejado);
        if (valor_digital_salvo == VALOR_DIGITAL_INVALIDO) {
            valor_digital_salvo = valor_digital;
            LOG("valor digital salvo - tabela[", volume_inteiro - FLUXO_MIN, "][", casa_decimal, "] = ", valor_digital);
        }

        valor_digital += incremento;
        if (valor_digital > 4095) {
            LOG("valor digital muito alto");
            break;
        }

        LOG("analise completa");
    }

    Bico::the().aplicar_forca(0);

    LOG("tabela ficou assim: ");
    for (size_t i = 0; i < tabela.size(); ++i) {
        auto& linha = tabela.at(i);
        for (size_t j = 0; j < linha.size(); ++j) {
            auto& valor_digital = linha.at(j);
            if (valor_digital != VALOR_DIGITAL_INVALIDO)
                LOG("tabela[", i, "][", j, "] (", i + FLUXO_MIN, ".", j, "g/s) = ", valor_digital);
        }
    }
}

uint32_t Bico::ControladorFluxo::melhor_valor_digital(float fluxo) {
    const auto volume_inteiro = static_cast<int>(fluxo);
    const auto casa_decimal = casa_decimal_apropriada(fluxo);

    const auto fluxo_index = volume_inteiro - FLUXO_MIN;
    // se tivermos um valor digital salvo, vamos usá-lo
    auto& valores_digitais = tabela[fluxo_index];
    if (valores_digitais[casa_decimal] != VALOR_DIGITAL_INVALIDO)
        return valores_digitais[casa_decimal];

    // se não tivermos, vamos procurar o valor digital mais próximo
    const auto [fluxo_abaixo, valor_digital_abaixo] = primeiro_fluxo_abaixo(fluxo_index, casa_decimal);
    const auto [fluxo_acima, valor_digital_acima] = primeiro_fluxo_acima(fluxo_index, casa_decimal);
    if (valor_digital_abaixo == VALOR_DIGITAL_INVALIDO || valor_digital_acima == VALOR_DIGITAL_INVALIDO)
        return VALOR_DIGITAL_INVALIDO;

    LOG("fluxo_abaixo = ", fluxo_abaixo, " | fluxo_acima = ", fluxo_acima, " | fluxo = ", fluxo);
    LOG("valor_digital_abaixo = ", valor_digital_abaixo, " | valor_digital_acima = ", valor_digital_acima);

    const auto normalizado = (fluxo - fluxo_abaixo) / (fluxo_acima - fluxo_abaixo);
    const auto interp = std::lerp(static_cast<float>(valor_digital_abaixo), static_cast<float>(valor_digital_acima), normalizado);
    const auto valor_digital = static_cast<uint32_t>(std::round(interp));
    LOG("RESULTADO = ", valor_digital);
    return valor_digital;
}

uint32_t Bico::ControladorFluxo::analisar_e_melhorar_fluxo(uint32_t valor_digital, float fluxo_desejado, uint64_t pulsos) {
    LOG("iniciando analise");

    const auto volume_despejado = static_cast<float>(pulsos) * ML_POR_PULSO;
    LOG("volume despejado = ", volume_despejado);
    auto& valor_salvo = valor_digital_para_fluxo(volume_despejado);
    const auto fluxo_index = static_cast<int>(volume_despejado);
    const auto casa_decimal = casa_decimal_apropriada(volume_despejado);
    if (valor_salvo == VALOR_DIGITAL_INVALIDO) {
        LOG("adicionando valor na tabela - tabela[", fluxo_index - FLUXO_MIN, "][", casa_decimal, "] = ", valor_digital);
        valor_salvo = valor_digital;
    }

    const auto [desejado_index, desejado_decimal] = decompor_fluxo(fluxo_desejado);
    LOG("fluxo decomposto = ", desejado_index, ".", desejado_decimal, "g/s");
    const auto delta = std::abs(fluxo_desejado - volume_despejado);
    // TEM QUE ARRUMAR ESSA MERDA DE CODIGO PQP DA PRA LER PORRA NENHUMA
    if (volume_despejado < fluxo_desejado) {
        const auto [fluxo_acima, valor_digital_acima] = primeiro_fluxo_acima(desejado_index, desejado_decimal);
        LOG("fluxo_acima = ", fluxo_acima, " | valor_digital_acima = ", valor_digital_acima);
        const auto normalizado = (delta - volume_despejado) / (fluxo_acima - volume_despejado);
        const auto interp = std::lerp(static_cast<float>(valor_digital), static_cast<float>(valor_digital_acima), normalizado);
        const auto valor_digital = static_cast<uint32_t>(std::round(interp));
        LOG("RESULTANTE DA ANALISE = ", valor_digital);
        LOG("finalizando analise");
        return valor_digital;
    } else if (volume_despejado > fluxo_desejado) {
        const auto [fluxo_abaixo, valor_digital_abaixo] = primeiro_fluxo_abaixo(desejado_index, desejado_decimal);
        LOG("fluxo_abaixo = ", fluxo_abaixo, " | valor_digital_abaixo = ", valor_digital_abaixo);
        const auto normalizado = (delta - fluxo_abaixo) / (volume_despejado - fluxo_abaixo);
        const auto interp = std::lerp(static_cast<float>(valor_digital_abaixo), static_cast<float>(valor_digital), normalizado);
        const auto valor_digital = static_cast<uint32_t>(std::round(interp));
        LOG("RESULTANTE DA ANALISE = ", valor_digital);
        LOG("finalizando analise");
        return valor_digital;
    } else {
        return valor_digital;
    }
}

std::tuple<float, uint32_t> Bico::ControladorFluxo::primeiro_fluxo_abaixo(int fluxo_index, int casa_decimal) {
    for (; fluxo_index >= 0; --fluxo_index, casa_decimal = 9) {
        auto& valores_digitais = tabela[fluxo_index];
        for (; casa_decimal >= 0; --casa_decimal) {
            if (valores_digitais[casa_decimal] != VALOR_DIGITAL_INVALIDO) {
                const auto fluxo = static_cast<float>(fluxo_index + FLUXO_MIN) + static_cast<float>(casa_decimal) / 10.f;
                return { fluxo, valores_digitais[casa_decimal] };
            }
        }
    }

    return { 0.f, VALOR_DIGITAL_INVALIDO };
}

std::tuple<float, uint32_t> Bico::ControladorFluxo::primeiro_fluxo_acima(int fluxo_index, int casa_decimal) {
    for (; fluxo_index < RANGE_FLUXO; ++fluxo_index, casa_decimal = 0) {
        auto& valores_digitais = tabela[fluxo_index];
        for (; casa_decimal < 10; ++casa_decimal) {
            if (valores_digitais[casa_decimal] != VALOR_DIGITAL_INVALIDO) {
                const auto fluxo = static_cast<float>(fluxo_index + FLUXO_MIN) + static_cast<float>(casa_decimal) / 10.f;
                return { fluxo, valores_digitais[casa_decimal] };
            }
        }
    }

    return { 0.f, VALOR_DIGITAL_INVALIDO };
}

int Bico::ControladorFluxo::casa_decimal_apropriada(float fluxo) {
    const auto casa_decimal_antes_de_arredondar = static_cast<int>(fluxo * 10.f) % 10;
    const auto volume_arredondado =
        (casa_decimal_antes_de_arredondar == 9
             // se a primeira casa decimal for 9 nós arredondamos para *baixo*, para continuarmos dentro do mesmo valor absoluto
             // por exemplo, 1.99 arredondado normalmente é 2.0, mas 1.9 arredondado para *baixo* é 1.9
             // ou seja, continuamos no "campo" de 1.0-1.9
             ? std::floor(fluxo * 10.f)
             : std::round(fluxo * 10.f)) /
        10.f;

    const auto casa_decimal = static_cast<int>(volume_arredondado * 10.f) % 10;

    return casa_decimal;
}

std::tuple<float, uint32_t> Bico::ControladorFluxo::decompor_fluxo(float fluxo) {
    const auto volume_inteiro = static_cast<int>(fluxo);
    const auto casa_decimal = casa_decimal_apropriada(fluxo);

    return { volume_inteiro + static_cast<float>(casa_decimal) / 10.f, valor_digital_para_fluxo(fluxo) };
}

uint32_t& Bico::ControladorFluxo::valor_digital_para_fluxo(float fluxo) {
    const auto volume_inteiro = static_cast<int>(fluxo);
    const auto casa_decimal = casa_decimal_apropriada(fluxo);

    return tabela[volume_inteiro - FLUXO_MIN][casa_decimal];
}
}
