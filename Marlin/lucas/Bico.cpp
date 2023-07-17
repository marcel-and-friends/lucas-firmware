#include "Bico.h"
#include <lucas/lucas.h>
#include <lucas/cmd/cmd.h>
#include <src/module/temperature.h>
#include <src/module/planner.h>
#include <cmath>

namespace lucas {
namespace pino {
constexpr auto ENABLE = PA3;
constexpr auto SV = PA5;
constexpr auto BREAK = PB2;
constexpr auto REF = PE6;
constexpr auto FLUXO = PA13;
};

void Bico::tick() {
    if (m_ativo) {
        m_tempo_decorrido = millis() - m_tick_comeco;
        if (m_tempo_decorrido >= m_duracao) {
            desligar();
            return;
        }

        if (m_tempo_decorrido > 1000) {
            if ((m_tempo_decorrido % 1000) == 0) {
                const auto volume_total_despejado = (s_contador_de_pulsos - m_pulsos_no_inicio_do_despejo) * ControladorFluxo::ML_POR_PULSO;
                if (m_volume_total_desejado) {
                    const auto fluxo_ideal = (m_volume_total_desejado - volume_total_despejado) / ((m_duracao - m_tempo_decorrido) / 1000);
                    aplicar_forca(ControladorFluxo::the().melhor_valor_digital(fluxo_ideal));
                }
            }
        }
    } else {
        // após desligar o motor deixamos o break ativo por um tempinho e depois liberamos
        constexpr auto TEMPO_PARA_DESLIGAR_O_BREAK = 2000; // 2s
        if (m_tick_final) {
            if (millis() - m_tick_final >= TEMPO_PARA_DESLIGAR_O_BREAK) {
                digitalWrite(pino::BREAK, HIGH);
                m_tick_final = 0;
                // const auto pulsos = static_cast<uint32_t>(s_contador_de_pulsos - m_pulsos_no_final_do_despejo);
                // LOG_IF(LogDespejoBico, "desligando break apos despejo - [delta = ", tick - m_tick_final, " | pulsos extras = ", pulsos, "]");
            }
        }
    }
}

void Bico::despejar_volume(millis_t duracao, float volume_desejado) {
    if (!duracao || !volume_desejado)
        return;

    iniciar_despejo(duracao);
    aplicar_forca(ControladorFluxo::the().melhor_valor_digital(volume_desejado / (duracao / 1000)));
    m_volume_total_desejado = volume_desejado;

    if (m_forca != 0)
        LOG_IF(LogDespejoBico, "iniciando despejo - [volume desejado = ", volume_desejado, " | forca = ", m_forca, "]");
}

void Bico::despejar_volume_e_aguardar(millis_t duracao, float volume_desejado) {
    despejar_volume(duracao, volume_desejado);
    util::aguardar_enquanto(
        [] { return Bico::the().ativo(); },
        Filtros::Interacao);
}

void Bico::desepejar_com_valor_digital(millis_t duracao, uint32_t valor_digital) {
    if (!duracao || !valor_digital)
        return;

    iniciar_despejo(duracao);
    aplicar_forca(valor_digital);
    LOG_IF(LogDespejoBico, "iniciando despejo - [forca = ", m_forca, "]");
}

void Bico::desligar() {
    const auto volume_despejado = (s_contador_de_pulsos - m_pulsos_no_inicio_do_despejo) * ControladorFluxo::ML_POR_PULSO;
    const auto pulsos = s_contador_de_pulsos - m_pulsos_no_inicio_do_despejo;
    LOG_IF(LogDespejoBico, "finalizando despejo - [delta = ", m_tempo_decorrido, "ms | volume = ", volume_despejado, " | pulsos = ", pulsos, "]");

    aplicar_forca(0);
    m_ativo = false;
    m_tick_comeco = 0;
    m_tick_final = millis();
    m_duracao = 0;
    m_volume_total_desejado = 0.f;
    m_tempo_decorrido = 0;
    m_pulsos_no_inicio_do_despejo = 0;
    m_pulsos_no_final_do_despejo = s_contador_de_pulsos;
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

    nivelar();

    pinMode(pino::FLUXO, INPUT);
    attachInterrupt(
        digitalPinToInterrupt(pino::FLUXO),
        +[] {
            ++s_contador_de_pulsos;
        },
        RISING);

    if (CFG(PreencherTabelaDeFluxoNoSetup))
        preencher_tabela_de_controle_de_fluxo();
}

void Bico::viajar_para_estacao(Estacao& estacao, int offset) const {
    viajar_para_estacao(estacao.index(), offset);
}

void Bico::viajar_para_estacao(Estacao::Index index, int offset) const {
    LOG_IF(LogViagemBico, "viajando - [estacao = ", index, " | offset = ", offset, "]");

    const auto comeco = millis();
    const auto movimento = util::ff("G0 F50000 Y60 X%s", Estacao::posicao_absoluta(index) + offset);

    cmd::executar_cmds("G90",
                       movimento,
                       "G91");

    aguardar_viagem_terminar();

    LOG_IF(LogViagemBico, "viagem completa - [duracao = ", millis() - comeco, "ms]");
}

void Fila::viajar_para_lado_da_estacao(Estacao&) const {
    viajar_para_lado_da_estacao(estacao.index());
}

void Fila::viajar_para_lado_da_estacao(Estacao::Index) const {
    const auto offset = -(util::distancia_entre_estacoes() / 2.f);
    viajar_para_estacao(estacao, offset);
}

void Bico::viajar_para_esgoto() const {
    LOG_IF(LogViagemBico, "viajando para o esgoto");

    const auto comeco = millis();
    cmd::executar_cmds("G90",
                       "G0 F50000 Y60 X5",
                       "G91");

    aguardar_viagem_terminar();

    LOG_IF(LogViagemBico, "viagem completa - [duracao = ", millis() - comeco, "ms]");
}

void Bico::aguardar_viagem_terminar() const {
    util::aguardar_enquanto(&Planner::busy, Filtros::Fila);
}

void Bico::nivelar() const {
    LOG("executando rotina de nivelamento");
    cmd::executar("G28 XY");
    LOG("nivelamento finalizado");
}

void Bico::preencher_tabela_de_controle_de_fluxo() const {
    ControladorFluxo::the().preencher_tabela();
}

void Bico::aplicar_forca(uint32_t v) {
    if (v == ControladorFluxo::VALOR_DIGITAL_INVALIDO) {
        LOG_ERR("tentando aplicar valor digital invalido, desligando");
        desligar();
        return;
    }
    m_forca = v;
    digitalWrite(pino::BREAK, !!m_forca);
    analogWrite(pino::SV, m_forca);
}

void Bico::iniciar_despejo(millis_t duracao) {
    m_ativo = true;
    m_tick_comeco = millis();
    m_duracao = duracao;
    m_pulsos_no_inicio_do_despejo = s_contador_de_pulsos;
}

void Bico::ControladorFluxo::preencher_tabela() {
    m_tabela[1][8] = 1200;
    m_tabela[1][9] = 1225;
    m_tabela[2][2] = 1250;
    m_tabela[2][3] = 1275;
    m_tabela[2][4] = 1325;
    m_tabela[2][5] = 1350;
    m_tabela[2][6] = 1375;
    m_tabela[2][9] = 1400;
    m_tabela[3][3] = 1450;
    m_tabela[3][4] = 1475;
    m_tabela[3][7] = 1500;
    m_tabela[3][9] = 1525;
    m_tabela[4][1] = 1550;
    m_tabela[4][4] = 1575;
    m_tabela[4][6] = 1600;
    m_tabela[4][9] = 1625;
    m_tabela[5][2] = 1650;
    m_tabela[5][8] = 1675;
    m_tabela[6][2] = 1688;
    m_tabela[7][1] = 1700;
    m_tabela[7][9] = 1706;
    m_tabela[8][5] = 1709;
    m_tabela[8][9] = 1712;
    m_tabela[9][6] = 1715;
    return;
    constexpr auto VALOR_DIGITAL_INICIAL = 1200;
    constexpr auto INCREMENTO_INICIAL = 25;
    constexpr auto TEMPO_DE_ANALIZAR_FLUXO = 1000 * 10;
    constexpr auto TEMPO_DE_PREENCHER_MANGUEIRA = 1000 * 20;

    limpar_tabela();

    Bico::the().viajar_para_esgoto();
    Bico::the().aplicar_forca(1650);
    util::aguardar_por(TEMPO_DE_PREENCHER_MANGUEIRA, Filtros::Interacao);

    static float ultimo_fluxo_medio = 0;

    int incremento = INCREMENTO_INICIAL;
    int valor_digital = VALOR_DIGITAL_INICIAL;
    while (true) {
        if (valor_digital >= 4095)
            break;

        const auto contador_comeco = s_contador_de_pulsos;

        LOG_IF(LogNivelamento, "aplicando forca de ", valor_digital);
        Bico::the().aplicar_forca(valor_digital);

        util::aguardar_por(TEMPO_DE_ANALIZAR_FLUXO, Filtros::Interacao);

        const auto pulsos = s_contador_de_pulsos - contador_comeco;
        const auto fluxo_medio = (float(pulsos) * ML_POR_PULSO) / (TEMPO_DE_ANALIZAR_FLUXO / 1000);

        LOG_IF(LogNivelamento, "fluxo estabilizou, vamos analisar - [pulsos = ", pulsos, " | fluxo_medio = ", fluxo_medio, "]");

        if (ultimo_fluxo_medio) {
            const auto delta = fluxo_medio - ultimo_fluxo_medio;
            if (delta > 1.f) {
                incremento = std::max(incremento / 2, 1);
                valor_digital -= incremento;
                LOG_IF(LogNivelamento, "pulo muito grande de força - [delta = ", delta, " | incremento = ", incremento);
                continue;
            }
        }

        if (fluxo_medio < FLUXO_MIN) {
            valor_digital += incremento;
            LOG_IF(LogNivelamento, "ainda nao temos fluxo o suficiente");
            continue;
        }

        if (fluxo_medio > FLUXO_MAX) {
            LOG_IF(LogNivelamento, "fluxo demais");
            break;
        }

        ultimo_fluxo_medio = fluxo_medio;
        const auto [volume_inteiro, casa_decimal] = decompor_fluxo(fluxo_medio);

        auto& valor_digital_salvo = valor_na_tabela(fluxo_medio);
        if (valor_digital_salvo == VALOR_DIGITAL_INVALIDO) {
            valor_digital_salvo = valor_digital;
            LOG_IF(LogNivelamento, "valor digital salvo - m_tabela[", volume_inteiro - FLUXO_MIN, "][", casa_decimal, "] = ", valor_digital);
        }

        valor_digital += incremento;

        LOG_IF(LogNivelamento, "analise completa");
    }

    Bico::the().desligar();

    LOG_IF(LogNivelamento, "resultado da m_tabela: ");
    for (size_t i = 0; i < m_tabela.size(); ++i) {
        auto& linha = m_tabela.at(i);
        for (size_t j = 0; j < linha.size(); ++j) {
            auto& valor_digital = linha.at(j);
            if (valor_digital != VALOR_DIGITAL_INVALIDO)
                LOG_IF(LogNivelamento, "m_tabela[", i, "][", j, "] = ", valor_digital, " - (", i + FLUXO_MIN, ".", j, "g/s");
        }
    }
}

uint32_t Bico::ControladorFluxo::melhor_valor_digital(float fluxo) {
    const auto [fluxo_inteiro, casa_decimal] = decompor_fluxo(fluxo);
    const auto fluxo_index = fluxo_inteiro - FLUXO_MIN;
    // se tivermos um valor digital salvo, vamos usá-lo
    auto& valores_digitais = m_tabela[fluxo_index];
    if (valores_digitais[casa_decimal] != VALOR_DIGITAL_INVALIDO)
        return valores_digitais[casa_decimal];

    // se não tivermos, vamos procurar o valor digital mais próximo
    const auto [fluxo_abaixo, valor_digital_abaixo] = primeiro_fluxo_abaixo(fluxo_index, casa_decimal);
    const auto [fluxo_acima, valor_digital_acima] = primeiro_fluxo_acima(fluxo_index, casa_decimal);

    if (valor_digital_abaixo == VALOR_DIGITAL_INVALIDO)
        return valor_digital_acima;
    else if (valor_digital_acima == VALOR_DIGITAL_INVALIDO)
        return valor_digital_abaixo;

    const auto normalizado = (fluxo - fluxo_abaixo) / (fluxo_acima - fluxo_abaixo);
    const auto interp = std::lerp(float(valor_digital_abaixo), float(valor_digital_acima), normalizado);
    const auto valor_digital = uint32_t(std::round(interp));
    return valor_digital;
}

void Bico::ControladorFluxo::limpar_tabela() {
    for (auto& valores_digitais : m_tabela)
        for (auto& valor : valores_digitais)
            valor = VALOR_DIGITAL_INVALIDO;
}

std::tuple<float, uint32_t> Bico::ControladorFluxo::primeiro_fluxo_abaixo(int fluxo_index, int casa_decimal) {
    for (; fluxo_index >= 0; --fluxo_index, casa_decimal = 9) {
        auto& valores_digitais = m_tabela[fluxo_index];
        for (; casa_decimal >= 0; --casa_decimal) {
            if (valores_digitais[casa_decimal] != VALOR_DIGITAL_INVALIDO) {
                const auto fluxo = float(fluxo_index + FLUXO_MIN) + float(casa_decimal) / 10.f;
                return { fluxo, valores_digitais[casa_decimal] };
            }
        }
    }

    return { 0.f, VALOR_DIGITAL_INVALIDO };
}

std::tuple<float, uint32_t> Bico::ControladorFluxo::primeiro_fluxo_acima(int fluxo_index, int casa_decimal) {
    for (; fluxo_index < RANGE_FLUXO; ++fluxo_index, casa_decimal = 0) {
        auto& valores_digitais = m_tabela[fluxo_index];
        for (; casa_decimal < 10; ++casa_decimal) {
            if (valores_digitais[casa_decimal] != VALOR_DIGITAL_INVALIDO) {
                const auto fluxo = float(fluxo_index + FLUXO_MIN) + float(casa_decimal) / 10.f;
                return { fluxo, valores_digitais[casa_decimal] };
            }
        }
    }

    return { 0.f, VALOR_DIGITAL_INVALIDO };
}

int Bico::ControladorFluxo::casa_decimal_apropriada(float fluxo) {
    const auto casa_decimal_antes_de_arredondar = int(fluxo * 10.f) % 10;
    const auto volume_arredondado =
        (casa_decimal_antes_de_arredondar == 9
             // se a primeira casa decimal for 9 nós arredondamos para *baixo*, para continuarmos dentro do mesmo valor absoluto
             // por exemplo, 1.99 arredondado normalmente é 2.0, mas 1.9 arredondado para *baixo* é 1.9
             // ou seja, continuamos no "campo" de 1.0-1.9
             ? std::floor(fluxo * 10.f)
             : std::round(fluxo * 10.f)) /
        10.f;

    const auto casa_decimal = int(volume_arredondado * 10.f) % 10;

    return casa_decimal;
}

std::tuple<int, uint32_t> Bico::ControladorFluxo::decompor_fluxo(float fluxo) {
    fluxo = std::clamp(fluxo, float(FLUXO_MIN), float(FLUXO_MAX) - 0.1f);
    return { fluxo, casa_decimal_apropriada(fluxo) };
}

uint32_t& Bico::ControladorFluxo::valor_na_tabela(float fluxo) {
    const auto [volume_inteiro, casa_decimal] = decompor_fluxo(fluxo);
    return m_tabela[volume_inteiro - FLUXO_MIN][casa_decimal];
}
}
