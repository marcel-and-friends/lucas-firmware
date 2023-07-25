#include "Estacao.h"
#include <memory>
#include <src/module/temperature.h>
#include <lucas/info/info.h>
#include <lucas/Bico.h>
#include <lucas/Fila.h>
#include <lucas/cmd/cmd.h>

namespace lucas {
Estacao::Lista Estacao::s_lista = {};

void Estacao::inicializar(size_t num) {
    if (num > NUM_MAX_ESTACOES) {
        LOG_ERR("numero de estacoes invalido - [max = ", NUM_MAX_ESTACOES, "]");
        return;
    }

    if (s_num_estacoes) {
        LOG_IF(LogEstacoes, "estacoes ja foram inicializadas");
        return;
    }

    struct InfoEstacao {
        int pino_botao;
        int pino_led;
    };

    constexpr std::array<InfoEstacao, Estacao::NUM_MAX_ESTACOES> infos = {
        InfoEstacao{.pino_botao = PC8,  .pino_led = PE13},
        InfoEstacao{ .pino_botao = PC4, .pino_led = PD13},
        InfoEstacao{ .pino_botao = PC4, .pino_led = PD13},
        InfoEstacao{ .pino_botao = PC4, .pino_led = PD13},
        InfoEstacao{ .pino_botao = PC4, .pino_led = PD13}
    };

    for (size_t i = 0; i < num; i++) {
        auto& info = infos.at(i);
        auto& estacao = s_lista.at(i);
        estacao.set_botao(info.pino_botao);
        estacao.set_led(info.pino_led);
        // todas as maquinas começam livres
        estacao.set_status(Status::Livre);
    }

    s_num_estacoes = num;

    LOG_IF(LogEstacoes, "maquina vai usar ", num, " estacoes");
}

void Estacao::tick() {
#if 0 // VOLTAR QUANDO TIVER FIACAO
	for_each_if(std::not_fn(&Estacao::bloqueada), [](Estacao& estacao) {
		const auto tick = millis();

		// atualizacao simples do estado do botao
		bool segurado_antes = estacao.botao_segurado();
		bool segurado_agora = util::segurando(estacao.botao());
		estacao.set_botao_segurado(segurado_agora);

		// agora vemos se o usuario quer cancelar a receita
		{
			constexpr auto TEMPO_PARA_CANCELAR_RECEITA = 3000; // 3s
			if (segurado_agora) {
				if (not estacao.tick_botao_segurado())
					estacao.set_tick_botao_segurado(tick);

				if (tick - estacao.tick_botao_segurado() >= TEMPO_PARA_CANCELAR_RECEITA) {
					Fila::the().cancelar_receita_da_estacao(estacao.index());
					estacao.set_receita_cancelada(true);
					return util::Iter::Continue;
				}
			}
		}

		// o botão acabou de ser solto, temos varias opcoes
		// TODO: mudar isso aqui pra usar um interrupt 'FALLING' no botao
			if (estacao.tick_botao_segurado()) {
				// logicamente o botao ja nao está mais sendo segurado (pois acabou de ser solto)
				estacao.set_tick_botao_segurado(0);
			}

			if (estacao.receita_cancelada()) {
				// se a receita acabou de ser cancelada, podemos voltar ao normal
				// o proposito disso é não enviar a receita padrao imediatamente após cancelar uma receita
				estacao.set_receita_cancelada(false);
				return util::Iter::Continue;
			}

			switch (estacao.status()) {
			case Status::Livre:
				Fila::the().agendar_receita_para_estacao(Receita::padrao(), estacao.index());
				break;
			case Status::ConfirmandoEscaldo:
			case Status::ConfirmandoAtaques:
				Fila::the().mapear_receita_para_estacao(estacao.index());
				break;
			case Status::Pronto:
				estacao.set_status(Status::Livre);
				break;
			default:
				break;
			}
		}

		return util::Iter::Continue;
	});
#endif
}

void Estacao::atualizar_leds() {
    constexpr auto INTERVALO_PISCADELA = 500;
    // é necessario manter um estado geral para que as leds pisquem juntas.
    // poderiamos simplificar essa funcao substituindo o 'WRITE(estacao.led(), ultimo_estado)' por 'TOGGLE(estacao.led())'
    // porém como cada estado dependeria do seu valor individual anterior as leds podem (e vão) sair de sincronia.
    static bool ultimo_estado = true;
    static millis_t ultimo_tick_atualizado = 0;

    if (millis() - ultimo_tick_atualizado >= INTERVALO_PISCADELA) {
        ultimo_estado = not ultimo_estado;
        ultimo_tick_atualizado = millis();
        for_each_if(&Estacao::aguardando_input, [](const Estacao& estacao) {
            // aguardando input do usuário - led piscando
            WRITE(estacao.led(), ultimo_estado);
            return util::Iter::Continue;
        });
    }
}

float Estacao::posicao_absoluta(size_t index) {
    return util::distancia_primeira_estacao() + index * util::distancia_entre_estacoes();
}

size_t Estacao::numero() const {
    return index() + 1;
}

Estacao::Index Estacao::index() const {
    // cute
    return ((uintptr_t)this - (uintptr_t)&s_lista) / sizeof(Estacao);
}

void Estacao::set_led(pin_t pino) {
    if (m_pino_led == pino)
        return;

    m_pino_led = pino;
    SET_OUTPUT(m_pino_led);
    WRITE(m_pino_led, LOW);
}

void Estacao::set_botao(pin_t pino) {
    if (m_pino_botao == pino)
        return;

    m_pino_botao = pino;
    SET_INPUT_PULLUP(m_pino_botao);
    WRITE(m_pino_botao, HIGH);
}

void Estacao::set_bloqueada(bool b) {
    if (m_bloqueada == b)
        return;

    m_bloqueada = b;
    LOG_IF(LogEstacoes, "estacao foi ", m_bloqueada ? "" : "des", "bloqueada", " - [index = ", index(), "]");
}

void Estacao::set_status(Status status, std::optional<uint32_t> receita_id) {
    if (m_status == status)
        return;

    m_status = status;

    info::evento(
        "novoStatus",
        [this, &receita_id](JsonObject o) {
            o["estacao"] = index();
            o["status"] = int(m_status);
            if (receita_id)
                o["receitaId"] = *receita_id;
        });

    switch (m_status) {
    case Status::Livre:
        WRITE(m_pino_led, LOW);
        return;
    case Status::Escaldando:
    case Status::FazendoCafe:
    case Status::Finalizando:
        WRITE(m_pino_led, HIGH);
        return;
    default:
        return;
    }
}
}
