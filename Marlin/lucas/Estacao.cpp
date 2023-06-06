#include "Estacao.h"
#include <memory>
#include <src/module/temperature.h>
#include <lucas/Bico.h>

#define ESTACAO_LOG(...) LOG("estacao #", this->numero(), " - ", "", __VA_ARGS__)

namespace lucas {
Estacao::Lista Estacao::s_lista = {};

void Estacao::iniciar(size_t num) {
    if (num > NUM_MAX_ESTACOES) {
        LOG("numero de estacoes invalido - [max = ", NUM_MAX_ESTACOES, "]");
        return;
    }

    if (s_num_estacoes) {
        LOG("estacoes ja iniciadas");
        return;
    }

    struct InfoEstacao {
        int pino_botao;
        int pino_led;
    };

    static constexpr std::array<InfoEstacao, Estacao::NUM_MAX_ESTACOES> infos = {
        InfoEstacao{.pino_botao = PC8,  .pino_led = PE13},
        InfoEstacao{ .pino_botao = PC4, .pino_led = PD13}
    };

    for (size_t i = 0; i < num; i++) {
        auto& info = infos.at(i);
        auto& estacao = Estacao::lista().at(i);
        estacao.set_botao(info.pino_botao);
        estacao.set_led(info.pino_led);
        // todas as maquinas começam livres
        estacao.set_livre(true);
    }

    s_num_estacoes = num;

    LOG("maquina vai usar ", num, " estacoes");
}
// TODO: desenvolver um algoritmo legal pra isso...
void Estacao::procurar_nova_ativa() {
    LOG("procurando nova estacao ativa...");

    bool achou = false;
    for_each([&achou](auto& estacao) {
        if (estacao.disponivel_para_uso()) {
            achou = true;
            set_estacao_ativa(&estacao);
            return util::Iter::Stop;
        }
        return util::Iter::Continue;
    });

    if (achou)
        return;

    LOG("nenhuma estacao esta disponivel :(");
    set_estacao_ativa(nullptr);
}

void Estacao::set_estacao_ativa(Estacao* estacao) {
    if (estacao == s_estacao_ativa)
        return;

    s_estacao_ativa = estacao;
    if (s_estacao_ativa) {
        auto& estacao = *s_estacao_ativa;
        estacao.atualizar_status(Status::MAKING_COFFEE);

        auto& bico = Bico::the();
        // bico.descartar_agua_ruim();
        bico.viajar_para_estacao(estacao);

#if LUCAS_DEBUG_GCODE
        LOG("-------- receita --------\n", estacao.receita().c_str() + estacao.progresso_receita());
        LOG("-------------------------");
#endif
        LOG("estacao #", estacao.numero(), " - escolhida como nova estacao ativa");
        UPDATE(LUCAS_UPDATE_ESTACAO_ATIVA, estacao.numero());
    } else {
        UPDATE(LUCAS_UPDATE_ESTACAO_ATIVA, "-");
        gcode::parar_fila();
    }
}

float Estacao::posicao_absoluta(size_t index) {
    constexpr auto DISTANCIA_PRIMEIRA_ESTACAO = 80.f;
    constexpr auto DISTANCIA_ENTRE_ESTACOES = 160.f;
    return DISTANCIA_PRIMEIRA_ESTACAO + index * DISTANCIA_ENTRE_ESTACOES;
}

void Estacao::enviar_receita(std::string receita, size_t id) {
    set_receita(std::move(receita), id);
    set_livre(false);
    set_aguardando_input(true);
}

void Estacao::prosseguir_receita() {
    auto instrucao = proxima_instrucao();
    atualizar_campo_gcode(CampoGcode::Atual, instrucao);
    if (gcode::ultima_instrucao(instrucao.data())) {
        atualizar_campo_gcode(CampoGcode::Proximo, "-");
        // podemos 'executar()' aqui pois, como é a ultima instrucao, a string é null-terminated
        gcode::executar(instrucao.data());
        reiniciar();
        ESTACAO_LOG("receita finalizada");
        atualizar_campo_gcode(CampoGcode::Atual, "-");
    } else {
        gcode::injetar(instrucao.data());
        m_receita_progresso += instrucao.size() + 1;
        atualizar_campo_gcode(CampoGcode::Proximo, proxima_instrucao());
    }
}

void Estacao::disponibilizar_para_uso() {
    if (m_receita_gcode.empty())
        return;

    set_livre(false);
    set_aguardando_input(false);
    if (!Estacao::ativa())
        Estacao::set_estacao_ativa(this);
    else
        atualizar_status(Status::IS_READY);
    ESTACAO_LOG("disponibilizada para uso");
}

void Estacao::cancelar_receita() {
    reiniciar();
    set_receita_cancelada(true);
    atualizar_campo_gcode(CampoGcode::Atual, "-");
    atualizar_campo_gcode(CampoGcode::Proximo, "-");
    ESTACAO_LOG("receita cancelada");
}

void Estacao::pausar(millis_t duracao) {
    m_pausada = true;
    m_comeco_pausa = millis();
    m_duracao_pausa = duracao;
    atualizar_status(Status::FREE);
    if (esta_ativa())
        procurar_nova_ativa();
    ESTACAO_LOG("pausando por ", duracao, "ms");
}

void Estacao::despausar() {
    m_pausada = false;
    m_comeco_pausa = 0;
    m_duracao_pausa = 0;
}

bool Estacao::tempo_de_pausa_atingido(millis_t tick) const {
    return tick - m_comeco_pausa >= m_duracao_pausa;
}

// isso aqui vai mudar quando tiver a fila
bool Estacao::disponivel_para_uso() const {
    return !bloqueada() && !esta_ativa() && !livre() && !aguardando_input() && !pausada();
}

bool Estacao::esta_ativa() const {
    return Estacao::ativa() == this;
}

void Estacao::atualizar_campo_gcode(CampoGcode qual, std::string_view valor) const {
    static char valor_buffer[64] = {};
    static char nome_buffer[] = "gCode?E?";

    nome_buffer[5] = static_cast<char>(qual) + '0';
    nome_buffer[7] = static_cast<char>(numero()) + '0';

    memcpy(valor_buffer, valor.data(), valor.size());
    valor_buffer[valor.size()] = '\0';

    UPDATE(nome_buffer, valor_buffer);
}

void Estacao::atualizar_status(Status status) const {
    static char nome_buffer[] = "statusE?";
    nome_buffer[7] = numero() + '0';
    UPDATE(nome_buffer, static_cast<int>(status));
}

size_t Estacao::numero() const {
    return index() + 1;
}

size_t Estacao::index() const {
    return ((uintptr_t)this - (uintptr_t)&s_lista) / sizeof(Estacao);
}

void Estacao::set_led(pin_t pino) {
    m_pino_led = pino;
    SET_OUTPUT(m_pino_led);
    WRITE(m_pino_led, LOW);
}

void Estacao::set_botao(pin_t pino) {
    m_pino_botao = pino;
    SET_INPUT_PULLUP(m_pino_botao);
    WRITE(m_pino_botao, HIGH);
}

void Estacao::set_aguardando_input(bool b) {
    m_aguardando_input = b;
    if (m_aguardando_input)
        atualizar_status(Status::WAITING_START);
}

void Estacao::set_receita(std::string gcode, size_t id) {
    m_receita_gcode = std::move(gcode);
    m_receita_progresso = 0;
    m_receita_id = id;
}

void Estacao::set_livre(bool b) {
    m_livre = b;
    if (m_livre)
        atualizar_status(Status::FREE);
}

void Estacao::set_bloqueada(bool b) {
    auto antigo = m_bloqueada;
    m_bloqueada = b;
    if (antigo != m_bloqueada) {
        ESTACAO_LOG(m_bloqueada ? "" : "des", "bloqueada");
        reiniciar();
    }
}

std::string_view Estacao::proxima_instrucao() const {
    return gcode::proxima_instrucao(m_receita_gcode.data() + m_receita_progresso);
}

void Estacao::reiniciar() {
    m_receita_gcode.clear();
    m_receita_progresso = 0;
    m_receita_id = 0;
    m_tick_botao_segurado = 0;
    m_botao_segurado = false;
    m_receita_cancelada = false;
    m_aguardando_input = false;
    m_pausada = false;
    m_comeco_pausa = 0;
    m_duracao_pausa = 0;
    set_livre(true);
    if (esta_ativa())
        procurar_nova_ativa();
}
}
