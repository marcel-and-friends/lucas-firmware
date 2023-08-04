#include "Bico.h"
#include <lucas/Estacao.h>
#include <lucas/lucas.h>
#include <lucas/cmd/cmd.h>
#include <src/module/temperature.h>
#include <src/module/planner.h>

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
                auto const volume_total_despejado = (s_contador_de_pulsos - m_pulsos_no_inicio_do_despejo) * ControladorFluxo::ML_POR_PULSO;
                if (m_volume_total_desejado and m_corrigir_fluxo_durante_despejo == CorrigirFluxo::Sim) {
                    auto const fluxo_ideal = (m_volume_total_desejado - volume_total_despejado) / ((m_duracao - m_tempo_decorrido) / 1000);
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

void Bico::despejar_volume(millis_t duracao, float volume_desejado, CorrigirFluxo corrigir) {
    if (duracao == 0 or volume_desejado == 0)
        return;

    iniciar_despejo(duracao);
    aplicar_forca(ControladorFluxo::the().melhor_valor_digital(volume_desejado / (duracao / 1000)));
    m_volume_total_desejado = volume_desejado;
    m_corrigir_fluxo_durante_despejo = corrigir;
    LOG_IF(LogDespejoBico, "iniciando despejo - [volume desejado = ", volume_desejado, " | forca = ", m_forca, "]");
}

float Bico::despejar_volume_e_aguardar(millis_t duracao, float volume_desejado, CorrigirFluxo corrigir) {
    despejar_volume(duracao, volume_desejado, corrigir);
    util::aguardar_enquanto([] { return Bico::the().ativo(); }, Filtros::Interacao);
    return (m_pulsos_no_final_do_despejo - m_pulsos_no_inicio_do_despejo) * ControladorFluxo::ML_POR_PULSO;
}

void Bico::despejar_valor_digital(millis_t duracao, uint32_t valor_digital) {
    if (duracao == 0 or valor_digital == 0)
        return;

    iniciar_despejo(duracao);
    aplicar_forca(valor_digital);
    LOG_IF(LogDespejoBico, "iniciando despejo - [forca = ", m_forca, "]");
}

void Bico::desligar() {
    aplicar_forca(0);
    m_ativo = false;
    m_tick_comeco = 0;
    m_tick_final = millis();
    m_duracao = 0;
    m_volume_total_desejado = 0.f;
    m_corrigir_fluxo_durante_despejo = CorrigirFluxo::Nao;

    m_pulsos_no_final_do_despejo = s_contador_de_pulsos;
    auto const volume_despejado = (m_pulsos_no_final_do_despejo - m_pulsos_no_inicio_do_despejo) * ControladorFluxo::ML_POR_PULSO;
    auto const pulsos = m_pulsos_no_final_do_despejo - m_pulsos_no_inicio_do_despejo;
    LOG_IF(LogDespejoBico, "finalizando despejo - [delta = ", m_tempo_decorrido, "ms | volume = ", volume_despejado, " | pulsos = ", pulsos, "]");

    m_tempo_decorrido = 0;
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

    pinMode(pino::FLUXO, INPUT);
    attachInterrupt(
        digitalPinToInterrupt(pino::FLUXO),
        +[] {
            ++s_contador_de_pulsos;
        },
        RISING);
}

void Bico::viajar_para_estacao(Estacao& estacao, float offset) const {
    viajar_para_estacao(estacao.index(), offset);
}

void Bico::viajar_para_estacao(size_t index, float offset) const {
    LOG_IF(LogViagemBico, "viajando - [estacao = ", index, " | offset = ", offset, "]");

    auto const comeco = millis();
    auto const movimento = util::ff("G0 F50000 Y60 X%s", Estacao::posicao_absoluta(index) + offset);
    cmd::executar_cmds("G90",
                       movimento,
                       "G91");

    aguardar_viagem_terminar();

    LOG_IF(LogViagemBico, "viagem completa - [duracao = ", millis() - comeco, "ms]");
}

void Bico::viajar_para_esgoto() const {
    LOG_IF(LogViagemBico, "viajando para o esgoto");

    auto const comeco = millis();
    cmd::executar_cmds("G90",
                       "G0 F5000 Y60 X5",
                       "G91");

    aguardar_viagem_terminar();

    LOG_IF(LogViagemBico, "viagem completa - [duracao = ", millis() - comeco, "ms]");
}

void Bico::aguardar_viagem_terminar() const {
    util::aguardar_enquanto(&Planner::busy, Filtros::Fila);
}

void Bico::setar_temperatura_boiler(float target) const {
    constexpr auto HYSTERESIS_INICIAL = 1.5f;
    constexpr auto HYSTERESIS_FINAL = 0.5f;

    thermalManager.setTargetBed(target);
    auto const temp_boiler = thermalManager.degBed();
    if (temp_boiler >= target)
        return;

    auto const delta = std::abs(target - temp_boiler);
    util::set_hysteresis(delta < HYSTERESIS_INICIAL ? HYSTERESIS_FINAL : HYSTERESIS_INICIAL);

    LOG_IF(LogNivelamento, "iniciando nivelamento de temperatura - [hysteresis = ", util::hysteresis(), "]");

    util::aguardar_enquanto([target] {
        constexpr auto INTERVALO_LOG = 5000;
        static auto ultimo_log = millis();
        if (millis() - ultimo_log >= INTERVALO_LOG) {
            LOG_IF(LogNivelamento, "info - [temp = ", thermalManager.degBed(), " | target = ", thermalManager.degTargetBed(), "]");
            ultimo_log = millis();
        }

        const auto delta = std::abs(target - thermalManager.degBed());
        return delta >= util::hysteresis();
    });

    LOG_IF(LogNivelamento, "temperatura desejada foi atingida");

    util::set_hysteresis(HYSTERESIS_FINAL);
}

void Bico::nivelar(float temp_target) {
    util::FiltroUpdatesTemporario f{ Filtros::Interacao };

    LOG_IF(LogNivelamento, "iniciando rotina de nivelamento");

    cmd::executar("G28 XY");

    if (CFG(SetarTemperaturaTargetNoNivelamento))
        setar_temperatura_boiler(temp_target);

    if (CFG(PreencherTabelaDeFluxoNoNivelamento))
        ControladorFluxo::the().preencher_tabela();
    else
        ControladorFluxo::the().tentar_copiar_tabela_da_flash();

    LOG_IF(LogNivelamento, "nivelamento finalizado");

    m_nivelado = true;
}

void Bico::aplicar_forca(uint32_t v) {
    if (v == ControladorFluxo::VALOR_DIGITAL_INVALIDO) {
        LOG_ERR("tentando aplicar valor digital invalido, desligando");
        desligar();
        return;
    }
    m_forca = v;
    digitalWrite(pino::BREAK, m_forca > 0 ? HIGH : LOW);
    analogWrite(pino::SV, m_forca);
}

void Bico::preencher_mangueira(float volume_desejado) {
    constexpr auto FALLBACK_FORCA = 1400;
    constexpr auto TEMPO_DE_PREENCHER_MANGUEIRA = 1000 * 20;

    Bico::the().viajar_para_esgoto();

    if (volume_desejado)
        despejar_volume(TEMPO_DE_PREENCHER_MANGUEIRA, volume_desejado, CorrigirFluxo::Sim);
    else
        despejar_valor_digital(TEMPO_DE_PREENCHER_MANGUEIRA, FALLBACK_FORCA);

    LOG_IF(LogNivelamento, "preenchendo a mangueira com agua - [duracao = ", TEMPO_DE_PREENCHER_MANGUEIRA / 1000, "s]");
    util::aguardar_por(TEMPO_DE_PREENCHER_MANGUEIRA);
}

void Bico::iniciar_despejo(millis_t duracao) {
    m_ativo = true;
    m_tick_comeco = millis();
    m_duracao = duracao;
    m_pulsos_no_inicio_do_despejo = s_contador_de_pulsos;
}

void Bico::ControladorFluxo::preencher_tabela() {
    constexpr auto VALOR_DIGITAL_INICIAL = 0;
    constexpr auto MODIFICACAO_VALOR_DIGITAL_INICIAL = 200;
    constexpr auto MODIFICACAO_VALOR_DIGITAL_APOS_OBTER_FLUXO_MINIMO = 25;
    constexpr auto DELTA_MAXIMO_ENTRE_DESPEJOS = 1.f;

    constexpr auto TEMPO_DE_ANALISAR_FLUXO = 1000 * 10;
    static_assert(TEMPO_DE_ANALISAR_FLUXO >= 1000, "tempo de analise deve ser no minimo 1 minuto");

    util::FiltroUpdatesTemporario f{ Filtros::Interacao };

    limpar_tabela(SalvarNaFlash::Nao);

    auto const inicio = millis();

    // prepara o bico na posição correta e preenche a mangueira com água quente, para evitarmos resultados erroneos
    Bico::the().preencher_mangueira();

    // valor enviado para o driver do motor
    int valor_digital = VALOR_DIGITAL_INICIAL;
    // modificacao aplicada nesse valor em cada iteração do nivelamento
    int modificacao_valor_digital = MODIFICACAO_VALOR_DIGITAL_INICIAL;
    // o fluxo medio do ultimo despejo, usado para calcularmos o delta entre despejos e modificarmos o valor digital apropriadamente
    float fluxo_medio_do_ultimo_despejo = 0.f;
    // flag para sabermos se ja temos o valor minimo e podemos prosseguir com o restante do nivelamento
    bool obteve_fluxo_minimo = false;
    // flag que diz se o ultimo despejo em busca do fluxo minimo foi maior ou menor que o fluxo minimo
    bool ultimo_maior_minimo = false;

    while (true) {
        // chegamos no limite, não podemos mais aumentar a força
        if (valor_digital >= 4095)
            break;

        auto const contador_comeco = s_contador_de_pulsos;

        LOG_IF(LogNivelamento, "aplicando forca = ", valor_digital);
        Bico::the().aplicar_forca(valor_digital);

        util::aguardar_por(TEMPO_DE_ANALISAR_FLUXO);

        auto const pulsos = s_contador_de_pulsos - contador_comeco;
        auto const fluxo_medio = (float(pulsos) * ML_POR_PULSO) / float(TEMPO_DE_ANALISAR_FLUXO / 1000);

        LOG_IF(LogNivelamento, "fluxo estabilizou - [pulsos = ", pulsos, " | fluxo_medio = ", fluxo_medio, "]");

        { // primeiro precisamos obter o fluxo minimo
            if (not obteve_fluxo_minimo) {
                auto const delta = fluxo_medio - float(FLUXO_MIN);
                // valores entre FLUXO_MIN - 0.1 e FLUXO_MIN + 0.1 são aceitados
                // caso contrario, aumentamos ou diminuimos o valor digital ate chegarmos no fluxo minimo
                if (delta > -0.1f and delta < 0.1f) {
                    obteve_fluxo_minimo = true;
                    m_tabela[0][0] = valor_digital;

                    LOG_IF(LogNivelamento, "fluxo minimo obtido - [valor_digital = ", valor_digital, "]");

                    modificacao_valor_digital = MODIFICACAO_VALOR_DIGITAL_APOS_OBTER_FLUXO_MINIMO;
                    valor_digital += modificacao_valor_digital;
                } else {
                    bool const maior_minimo = fluxo_medio > float(FLUXO_MIN);
                    if (maior_minimo != ultimo_maior_minimo) {
                        if (modificacao_valor_digital < 0)
                            modificacao_valor_digital = std::min(modificacao_valor_digital / 2, -1);
                        else
                            modificacao_valor_digital = std::max(modificacao_valor_digital / 2, 1);
                        modificacao_valor_digital *= -1;
                    }

                    valor_digital += modificacao_valor_digital;

                    LOG_IF(LogNivelamento, "fluxo minimo nao obtido - ", maior_minimo ? "diminuindo" : "aumentando", " o valor digital para ", valor_digital, " - [modificacao = ", modificacao_valor_digital, "]");

                    ultimo_maior_minimo = maior_minimo;
                }
                continue;
            }
        }

        // se o fluxo diminui entre um despejo e outro o resultado é ignorado
        if (fluxo_medio_do_ultimo_despejo and fluxo_medio < fluxo_medio_do_ultimo_despejo) {
            valor_digital += modificacao_valor_digital;
            LOG_IF(LogNivelamento, "fluxo diminuiu?! aumentando valor digital - [fluxo_medio_do_ultimo_despejo = ", fluxo_medio_do_ultimo_despejo, "]");
            continue;
        }

        // se o fluxo ultrapassa o limite máximo, finalizamos o nivelamento
        if (fluxo_medio >= FLUXO_MAX) {
            LOG_IF(LogNivelamento, "chegamos no fluxo maximo, finalizando nivelamento");
            break;
        }

        { // precisamos garantir que o fluxo não deu um pulo muito grande entre esse e o último despejo, para termos uma quantidade maior de valores salvos na tabela
            if (fluxo_medio_do_ultimo_despejo) {
                auto const delta = fluxo_medio - fluxo_medio_do_ultimo_despejo;
                // o pulo foi muito grande, corta a modificacao no meio e reduz o valor digital
                if (delta > DELTA_MAXIMO_ENTRE_DESPEJOS) {
                    modificacao_valor_digital = std::max(modificacao_valor_digital / 2, 1);
                    valor_digital -= modificacao_valor_digital;
                    LOG_IF(LogNivelamento, "pulo muito grande de força - [delta = ", delta, " | nova modificacao = ", modificacao_valor_digital, "]");
                    continue;
                }
            }
        }

        { // salvamos o fluxo e o valor digital, aplicamos a modificação e prosseguimos com o proximo despejo
            fluxo_medio_do_ultimo_despejo = fluxo_medio;
            if (valor_na_tabela(fluxo_medio) == VALOR_DIGITAL_INVALIDO) {
                set_valor_na_tabela(fluxo_medio, valor_digital);
                LOG_IF(LogNivelamento, "valor digital salvo - [valor = ", valor_digital, "]");
            }

            valor_digital += modificacao_valor_digital;

            LOG_IF(LogNivelamento, "analise completa");
        }
    }

    Bico::the().desligar();

    LOG_IF(LogNivelamento, "tabela preenchida - [duracao = ", (millis() - inicio) / 60000.f, "min | numero de celulas = ", numero_celulas(), "]");

    LOG_IF(LogNivelamento, "resultado da tabela: ");
    for_each_celula([](uint32_t valor_digital, size_t i, size_t j) {
        LOG_IF(LogNivelamento, "m_tabela[", i, "][", j, "] = ", valor_digital, " = ", i + FLUXO_MIN, ".", j, "g/s");
        return util::Iter::Continue;
    });

    salvar_tabela_na_flash();
}

void Bico::ControladorFluxo::tentar_copiar_tabela_da_flash() {
    util::fill_flash_buffer();

    uint32_t valor_salvo_fluxo_minimo = util::buffered_read_flash<uint32_t>(0);
    if (valor_salvo_fluxo_minimo == 0 or
        valor_salvo_fluxo_minimo == VALOR_DIGITAL_INVALIDO or
        valor_salvo_fluxo_minimo > 4095)
        // nao temos um valor salvo
        return;

    LOG_IF(LogNivelamento, "tabela de fluxo sera copiada da flash - [minimo = ", valor_salvo_fluxo_minimo, "]");
    copiar_tabela_da_flash();
}

uint32_t Bico::ControladorFluxo::melhor_valor_digital(float fluxo) const {
    auto const [fluxo_inteiro, casa_decimal] = decompor_fluxo(fluxo);
    auto const fluxo_index = fluxo_inteiro - FLUXO_MIN;
    // se tivermos um valor digital salvo, vamos usá-lo
    auto& valores_digitais = m_tabela[fluxo_index];
    if (valores_digitais[casa_decimal] != VALOR_DIGITAL_INVALIDO)
        return valores_digitais[casa_decimal];

    // se não tivermos, vamos procurar o valor digital mais próximo
    auto const [fluxo_abaixo, valor_digital_abaixo] = primeiro_fluxo_abaixo(fluxo_index, casa_decimal);
    auto const [fluxo_acima, valor_digital_acima] = primeiro_fluxo_acima(fluxo_index, casa_decimal);

    if (valor_digital_abaixo == VALOR_DIGITAL_INVALIDO) {
        return valor_digital_acima;
    } else if (valor_digital_acima == VALOR_DIGITAL_INVALIDO) {
        return valor_digital_abaixo;
    }

    auto const normalizado = (fluxo - fluxo_abaixo) / (fluxo_acima - fluxo_abaixo);
    auto const interp = std::lerp(float(valor_digital_abaixo), float(valor_digital_acima), normalizado);
    auto const valor_digital = uint32_t(std::round(interp));
    return valor_digital;
}

void Bico::ControladorFluxo::limpar_tabela(SalvarNaFlash salvar) {
    for (auto& t : m_tabela)
        for (auto& v : t)
            v = VALOR_DIGITAL_INVALIDO;

    if (salvar == SalvarNaFlash::Sim)
        salvar_tabela_na_flash();
}

void Bico::ControladorFluxo::salvar_tabela_na_flash() {
    auto const bloco_tabela = reinterpret_cast<uint32_t*>(&m_tabela);
    for (size_t i = 0; i < sizeof(m_tabela) / sizeof(uint32_t); ++i)
        util::buffered_write_flash<uint32_t>(i * sizeof(uint32_t), bloco_tabela[i]);

    util::flush_flash();
}

void Bico::ControladorFluxo::copiar_tabela_da_flash() {
    util::fill_flash_buffer();

    auto bloco_tabela = reinterpret_cast<uint32_t*>(&m_tabela);
    for (size_t i = 0; i < sizeof(m_tabela) / sizeof(uint32_t); ++i)
        bloco_tabela[i] = util::buffered_read_flash<uint32_t>(i * sizeof(uint32_t));
}

std::tuple<float, uint32_t> Bico::ControladorFluxo::primeiro_fluxo_abaixo(int fluxo_index, int casa_decimal) const {
    for (; fluxo_index >= 0; --fluxo_index, casa_decimal = 9) {
        auto& valores_digitais = m_tabela[fluxo_index];
        for (; casa_decimal >= 0; --casa_decimal) {
            if (valores_digitais[casa_decimal] != VALOR_DIGITAL_INVALIDO) {
                auto const fluxo = float(fluxo_index + FLUXO_MIN) + float(casa_decimal) / 10.f;
                return { fluxo, valores_digitais[casa_decimal] };
            }
        }
    }

    return { 0.f, VALOR_DIGITAL_INVALIDO };
}

std::tuple<float, uint32_t> Bico::ControladorFluxo::primeiro_fluxo_acima(int fluxo_index, int casa_decimal) const {
    for (; fluxo_index < RANGE_FLUXO; ++fluxo_index, casa_decimal = 0) {
        auto& valores_digitais = m_tabela[fluxo_index];
        for (; casa_decimal < 10; ++casa_decimal) {
            if (valores_digitais[casa_decimal] != VALOR_DIGITAL_INVALIDO) {
                auto const fluxo = float(fluxo_index + FLUXO_MIN) + float(casa_decimal) / 10.f;
                return { fluxo, valores_digitais[casa_decimal] };
            }
        }
    }

    return { 0.f, VALOR_DIGITAL_INVALIDO };
}

int Bico::ControladorFluxo::casa_decimal_apropriada(float fluxo) const {
    auto const casa_decimal_antes_de_arredondar = int(fluxo * 10.f) % 10;
    auto const volume_arredondado =
        (casa_decimal_antes_de_arredondar == 9
             // se a primeira casa decimal for 9 nós arredondamos para *baixo*, para continuarmos dentro do mesmo valor absoluto
             // por exemplo, 1.99 arredondado normalmente é 2.0, mas 1.9 arredondado para *baixo* é 1.9
             // ou seja, continuamos no "campo" de 1.0-1.9
             ? std::floor(fluxo * 10.f)
             : std::round(fluxo * 10.f)) /
        10.f;

    return int(volume_arredondado * 10.f) % 10;
}

std::tuple<int, uint32_t> Bico::ControladorFluxo::decompor_fluxo(float fluxo) const {
    auto const fluxo_no_range = std::clamp(fluxo, float(FLUXO_MIN), float(FLUXO_MAX) - 0.1f);
    return { fluxo_no_range, casa_decimal_apropriada(fluxo_no_range) };
}

uint32_t Bico::ControladorFluxo::valor_na_tabela(float fluxo) const {
    auto const [volume_inteiro, casa_decimal] = decompor_fluxo(fluxo);
    return m_tabela[volume_inteiro - FLUXO_MIN][casa_decimal];
}

void Bico::ControladorFluxo::set_valor_na_tabela(float fluxo, uint32_t valor_digital) {
    auto const [volume_inteiro, casa_decimal] = decompor_fluxo(fluxo);
    m_tabela[volume_inteiro - FLUXO_MIN][casa_decimal] = valor_digital;
}
}
