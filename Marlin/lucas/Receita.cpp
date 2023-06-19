#include "Receita.h"
#include <lucas/cmd/cmd.h>
#include <lucas/lucas.h>
#include <lucas/Fila.h>
#include <numeric>

namespace lucas {
// FIXME isso seria melhor sendo uma referencia unica ao inves de inicializar um objeto novo toda vez que essa função é chamada
std::unique_ptr<Receita> Receita::padrao() {
    auto receita_padrao = std::make_unique<Receita>();
    receita_padrao->m_tempo_de_notificacao = 60000;
    receita_padrao->m_id = 0xF0F0; // >.<

    { // escaldo
        receita_padrao->m_escaldo = Passo{ .duracao = 6000 };
        strcpy(receita_padrao->m_escaldo->gcode, "L3 F10250 D9 N3 R1 T6000 A1560");
    }

    { // ataques
        constexpr auto passos = std::array{
            Passo{ .comeco_rel = 0, .duracao = 6000, .intervalo = 24000 },
            Passo{ .comeco_rel = 30000, .duracao = 9000, .intervalo = 30000 },
            Passo{ .comeco_rel = 69000, .duracao = 10000, .intervalo = 35000 },
            Passo{ .comeco_rel = 114000, .duracao = 10000 }
        };
        constexpr auto gcodes = std::array{
            "L3 F8250 D7 N3 R1 T6000 A1560",
            "L3 F8500 D7 N5 R1 T9000 A1560",
            "L3 F7650 D7 N5 R1 T10000 A1560",
            "L3 F7650 D7 N5 R1 T10000 A1560"
        };
        static_assert(passos.size() == gcodes.size());

        // isso aqui é feito dessa forma BURRA com o loop e os dois arrays pq o gcc simplesmente não consegue fazer aggreggate initialization de um array de char
        // https://stackoverflow.com/questions/70172941/c99-designator-member-outside-of-aggregate-initializer
        for (size_t i = 0; i < passos.size(); ++i) {
            receita_padrao->m_ataques[i] = passos[i];
            strcpy(receita_padrao->m_ataques[i].gcode, gcodes[i]);
        }

        receita_padrao->m_num_ataques = passos.size();
        receita_padrao->m_tempo_total = std::accumulate(passos.begin(), passos.end(), 0, [](auto acc, auto passo) {
            return acc + passo.duracao + passo.intervalo;
        });
    }

    return receita_padrao;
}

std::unique_ptr<Receita> Receita::from_json(JsonObjectConst json) {
    if (!json.containsKey("estacao") ||
        !json.containsKey("id") ||
        !json.containsKey("tempoTotal") ||
        !json.containsKey("notificacao") ||
        !json.containsKey("ataques")) {
        LOG("ERRO - json de receita nao possui todos os campos obrigatorios");
        return nullptr;
    }

    // yum have, a problem  in yum brain
    auto rec = std::make_unique<Receita>();

    { // informacoes gerais (e obrigatorias)
        rec->m_id = json["id"].as<size_t>();
        rec->m_tempo_total = json["tempoTotal"].as<millis_t>();
        rec->m_tempo_de_notificacao = json["notificacao"].as<millis_t>();
    }

    { // escaldo
        if (json.containsKey("escaldo")) {
            auto escaldo_obj = json["escaldo"].as<JsonObjectConst>();
            rec->m_escaldo = Passo{
                .duracao = escaldo_obj["duracao"].as<millis_t>(),
            };
            // c-string bleeehh
            strcpy(rec->m_escaldo->gcode, escaldo_obj["gcode"].as<const char*>());
        }
    }

    { // ataques
        auto ataques_obj = json["ataques"].as<JsonArrayConst>();
        for (size_t i = 0; i < ataques_obj.size(); ++i) {
            auto ataque_obj = ataques_obj[i];
            auto& ataque = rec->m_ataques[i];
            ataque.comeco_rel = ataque_obj["comeco"].as<millis_t>();
            ataque.duracao = ataque_obj["duracao"].as<millis_t>();
            strcpy(ataque.gcode, ataque_obj["gcode"].as<const char*>());
            if (ataque_obj.containsKey("intervalo")) {
                ataque.intervalo = ataque_obj["intervalo"].as<millis_t>();
            } else {
                // um numero "invalido", significa que esse ataque é o ultimo
                ataque.intervalo = static_cast<std::uint32_t>(-1);
            }
        }
        rec->m_num_ataques = ataques_obj.size();
    }

    return rec;
}

Receita::Acabou Receita::executar_passo() {
    if (Fila::the().estacao_ativa() == Estacao::INVALIDA) {
        LOG("ERRO - tentando executar o passo de uma receita sem uma estacao ativa");
        return Acabou::Nao;
    }

    auto& estacao = Estacao::lista().at(Fila::the().estacao_ativa());
    if (m_escaldo.has_value() && !m_escaldou) {
        estacao.set_status(Estacao::Status::SCALDING);
        GcodeSuite::process_subcommands_now(F(m_escaldo->gcode));
        estacao.set_status(Estacao::Status::INITIALIZE_COFFEE);

        m_escaldou = true;
        return Acabou::Nao;
    }

    estacao.set_status(Estacao::Status::MAKING_COFFEE);
    GcodeSuite::process_subcommands_now(F(passo_atual().gcode));

    return ++m_ataque_atual >= m_num_ataques ? Acabou::Sim : Acabou::Nao;
}

void Receita::mapear_passos_pendentes(millis_t tick) {
    for_each_passo_pendente([&](auto& passo) {
        passo.comeco_abs = tick;
        tick += passo.duracao + passo.intervalo;
        return util::Iter::Continue;
    });
}

bool Receita::passos_pendentes_estao_mapeados() {
    bool estao = true;
    for_each_passo_pendente([&](auto& passo) {
        if (passo.comeco_abs == 0) {
            estao = false;
            return util::Iter::Break;
        }
        return util::Iter::Continue;
    });
    return estao;
}

const Receita::Passo& Receita::passo_atual() const {
    if (m_escaldo.has_value() && !m_escaldou)
        return m_escaldo.value();
    return m_ataques[m_ataque_atual];
}

Receita::Passo& Receita::passo_atual() {
    if (m_escaldo.has_value() && !m_escaldou)
        return m_escaldo.value();
    return m_ataques[m_ataque_atual];
}

size_t Receita::passo_atual_idx() const {
    if (m_escaldo.has_value() && !m_escaldou)
        return 0;
    return m_ataque_atual + 1;
}
}
