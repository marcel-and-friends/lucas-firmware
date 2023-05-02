#include "Estacao.h"
#include <memory>
#include <src/module/temperature.h>

#define ESTACAO_LOG(...) LOG("estacao #", this->numero(), " - ", __VA_ARGS__)

namespace lucas {

Estacao::Lista Estacao::s_lista = {};

// TODO: desenvolver um algoritmo legal pra isso...
void Estacao::procurar_nova_ativa() {
    LOG("procurando nova estacao ativa...");

    for (auto& estacao : s_lista)
        if (estacao.disponivel_para_uso())
            return set_estacao_ativa(&estacao);

    LOG("nenhuma estacao esta disponivel :(");
    set_estacao_ativa(nullptr);
}

void Estacao::set_estacao_ativa(Estacao* estacao) {
    if (estacao == s_estacao_ativa)
        return;

    s_estacao_ativa = estacao;
    if (s_estacao_ativa) {
        auto& estacao = *s_estacao_ativa;
        LOG("estacao #", estacao.numero(), " - escolhida como nova estacao ativa");
#if LUCAS_ROTINA_TROCA
        executar_rotina_troca_de_estacao_ativa();
#endif
#if LUCAS_DEBUG_GCODE
        auto gcode = estacao.receita().c_str() + estacao.progresso_receita();
        LOG("-------- receita --------\n", gcode);
        LOG("-------------------------");
#endif
    } else {
        UPDATE(LUCAS_UPDATE_ESTACAO_ATIVA, "-");
        gcode::parar_fila();
    }
}

void Estacao::ativa_prestes_a_comecar() {
    s_trocando_estacao_ativa = false;
    LOG("troca de estacao finalizada, bico esta na nova posicao");
    auto& estacao = *Estacao::ativa();
    UPDATE(LUCAS_UPDATE_ESTACAO_ATIVA, estacao.numero());
    estacao.atualizar_status("Executando");
}

void Estacao::prosseguir_receita() {
    executar_instrucao(proxima_instrucao());
}

void Estacao::finalizar_receita() {
    m_receita.clear();
    m_progresso_receita = 0;
    set_livre(true);
    set_aguardando_input(false);
}

void Estacao::disponibilizar_para_uso() {
    ESTACAO_LOG("disponibilizada para uso");
    set_livre(false);
    set_aguardando_input(false);
    m_pausada = false;
    if (!Estacao::ativa())
        Estacao::set_estacao_ativa(this);
    else
        atualizar_status("Na fila");
}

void Estacao::aguardar_input() {
    m_aguardando_input = true;
    atualizar_status("Aguardando input");
}

void Estacao::cancelar_receita() {
    finalizar_receita();
    if (esta_ativa())
        Estacao::set_estacao_ativa(nullptr);

    atualizar_status("Livre");
    atualizar_campo_gcode(CampoGcode::Atual, "-");
    atualizar_campo_gcode(CampoGcode::Proximo, "-");
}

void Estacao::pausar(millis_t duracao) {
    m_pausada = true;
    m_comeco_pausa = millis();
    m_duracao_pausa = duracao;
    LOG("pausando por ", duracao, "ms");
    atualizar_status("Pausada");
}

bool Estacao::tempo_de_pausa_atingido(millis_t tick) const {
    return tick - m_comeco_pausa >= m_duracao_pausa;
}

bool Estacao::disponivel_para_uso() const {
    return !esta_ativa() && !livre() && !aguardando_input() && !pausada();
}

bool Estacao::esta_ativa() const {
    return Estacao::ativa() == this;
}

void Estacao::atualizar_campo_gcode(CampoGcode qual, std::string_view valor) const {
    static char valor_buffer[64] = {};
    static char nome_buffer[] = "gCode?E?";

    nome_buffer[5] = static_cast<char>(qual) + '0';
    nome_buffer[7] = numero() + '0';

    memcpy(valor_buffer, valor.data(), valor.size());
    valor_buffer[valor.size()] = '\0';
    UPDATE(nome_buffer, valor_buffer);
}

void Estacao::atualizar_status(std::string_view str) const {
    static char nome_buffer[] = "statusE?";
    nome_buffer[7] = numero() + '0';
    UPDATE(nome_buffer, str.data());
}

float Estacao::posicao_absoluta() const {
    static constexpr auto distancia_primeira_estacao = 80.f;
    static constexpr auto distancia_entre_estacoes = 160.f;
    return distancia_primeira_estacao + index() * distancia_entre_estacoes;
}

size_t Estacao::numero() const {
    return index() + 1;
}

size_t Estacao::index() const {
    return ((uintptr_t)this - (uintptr_t)&s_lista) / sizeof(Estacao);
}

void Estacao::set_aguardando_input(bool b) {
    if (b) {
        aguardar_input();
    } else {
        m_aguardando_input = false;
    }
}

namespace util {
}

void Estacao::executar_rotina_troca_de_estacao_ativa() {
    static std::string rotina = "";
    rotina.clear();

    static constexpr auto temp_ideal = 90.f;
    static constexpr auto margem_erro_temp = 10.f;

    // a segunda linha desse gcode indica a posição que será feita o descarte da água
    static constexpr auto descartar_agua_e_aguardar_temp_ideal =
        R"(G0 F50000 Y60 X10
G4 P3000
G0 F1000 E100
M109 T0 R%s\n)";

    gcode::adicionar_instrucao(rotina, "G90\n");

#if LUCAS_ROTINA_TEMP
    // se a temperatura não é ideal (dentro da margem de erro) nós temos que regulariza-la antes de começarmos a receita
    if (fabs(thermalManager.degHotend(0) - temp_ideal) >= margem_erro_temp)
        gcode::adicionar_instrucao_float(rotina, descartar_agua_e_aguardar_temp_ideal, temp_ideal);
#endif

    gcode::adicionar_instrucao_float(rotina, "G0 F50000 Y60 X%s\n", ativa()->posicao_absoluta());
    gcode::adicionar_instrucao(rotina, "G91");

    gcode::injetar(rotina);

    s_trocando_estacao_ativa = true;
    LOG("executando rotina da troca de estacao ativa");
#if LUCAS_DEBUG_GCODE
    LOG("---- gcode da rotina ----\n", rotina.c_str());
    LOG("-------------------------");
#endif
}

std::string_view Estacao::proxima_instrucao() const {
    return gcode::proxima_instrucao(m_receita.data() + m_progresso_receita);
}

void Estacao::executar_instrucao(std::string_view instrucao) {
    gcode::injetar(instrucao);
    atualizar_campo_gcode(CampoGcode::Atual, instrucao);
    if (gcode::e_ultima_instrucao(instrucao)) {
        atualizar_campo_gcode(CampoGcode::Proximo, "-");
        finalizar_receita();
    } else {
        m_progresso_receita += instrucao.size() + 1;
        atualizar_campo_gcode(CampoGcode::Proximo, proxima_instrucao());
    }
}
}
