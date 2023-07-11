#include "Receita.h"
#include <lucas/cmd/cmd.h>
#include <lucas/lucas.h>
#include <lucas/Fila.h>
#include <lucas/info/info.h>
#include <numeric>

namespace lucas {
// FIXME isso seria melhor sendo uma referencia unica ao inves de inicializar um objeto novo toda vez que essa função é chamada
JsonObjectConst Receita::padrao() {
    static StaticJsonDocument<512> doc;
    static char json[] = R"({
    "id": 61680,
    "tempoNotificacao": 60000,
    "escaldo": {
      "duracao": 6000,
      "gcode": "L3 D9 N3 R1 T6000 G80"
    },
    "ataques": [
      {
        "comeco": 0,
        "duracao": 6000,
        "intervalo": 24000,
        "gcode": "L3 D7 N3 R1 T6000 G60"
      }
    ]
  })";

    static bool once = false;
    if (!once) {
        deserializeJson(doc, json, sizeof(json));
        once = true;
    }

    return doc.as<JsonObjectConst>();
}

// TODO: alocar as receitas estaticamente no array da Fila e só modificar os dados quando necessario

bool Receita::Passo::colide_com(const Passo& outro) const {
    // dois ataques colidem se:

    // 1. um comeca dentro do outro
    if (this->comeco_abs >= outro.comeco_abs && this->comeco_abs <= outro.fim())
        return true;

    // 2. um termina dentro do outro
    if (this->fim() >= outro.comeco_abs && this->fim() <= outro.fim())
        return true;

    // 3. a distancia entre o comeco de um e o fim do outro é menor que o tempo de viagem de uma estacao à outra
    if (this->comeco_abs > outro.fim() && this->comeco_abs - outro.fim() < util::MARGEM_DE_VIAGEM)
        return true;

    // 4. a distancia entre o fim de um e o comeco do outro é menor que o tempo de viagem de uma estacao à outra
    if (this->fim() < outro.comeco_abs && outro.comeco_abs - this->fim() < util::MARGEM_DE_VIAGEM)
        return true;

    return false;
}

void Receita::montar_com_json(JsonObjectConst json) {
    { // informacoes gerais (e obrigatorias)
        this->m_id = json["id"].as<size_t>();
        this->m_tempo_de_finalizacao = json["tempoNotificacao"].as<millis_t>();
    }

    { // escaldo
        if (json.containsKey("escaldo")) {
            auto escaldo_obj = json["escaldo"].as<JsonObjectConst>();
            this->m_escaldo = Passo{
                .duracao = escaldo_obj["duracao"].as<millis_t>(),
            };
            // c-string bleeehh
            strcpy(this->m_escaldo->gcode, escaldo_obj["gcode"].as<const char*>());
        }
    }

    { // ataques
        auto ataques_obj = json["ataques"].as<JsonArrayConst>();
        for (size_t i = 0; i < ataques_obj.size(); ++i) {
            auto ataque_obj = ataques_obj[i];
            auto& ataque = this->m_ataques[i];
            ataque.duracao = ataque_obj["duracao"].as<millis_t>();
            strcpy(ataque.gcode, ataque_obj["gcode"].as<const char*>());
            if (ataque_obj.containsKey("intervalo")) {
                ataque.intervalo = ataque_obj["intervalo"].as<millis_t>();
            } else {
                // um numero "invalido", significa que esse ataque é o ultimo
                ataque.intervalo = 0;
            }
        }
        this->m_num_ataques = ataques_obj.size();
    }
}

void Receita::resetar() {
    m_id = 0;

    m_escaldou = false;
    m_escaldo.reset();

    m_tempo_de_finalizacao = 0;
    m_inicio_tempo_finalizacao = 0;

    m_num_ataques = 0;
    m_ataques = {};
    m_ataque_atual = 0;
}

bool Receita::executar_passo_atual() {
    if (m_escaldo.has_value() && !m_escaldou) {
        GcodeSuite::process_subcommands_now(F(m_escaldo->gcode));
        m_escaldou = true;
        return false;
    }
    GcodeSuite::process_subcommands_now(F(passo_atual().gcode));
    return ++m_ataque_atual >= m_num_ataques;
}

void Receita::mapear_passos_pendentes(millis_t tick) {
    for_each_passo_pendente([&](Passo& passo, size_t i) {
        passo.comeco_abs = tick;
        tick += passo.duracao + passo.intervalo;
        return util::Iter::Continue;
    });
}

void Receita::desmapear_passos() {
    for_each_passo([](Passo& passo) {
        passo.comeco_abs = 0;
        return util::Iter::Continue;
    });
}

bool Receita::passos_pendentes_estao_mapeados() const {
    bool estao = false;
    for_each_passo_pendente([&](auto& passo) {
        if (passo.comeco_abs != 0) {
            estao = true;
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
    return m_ataque_atual + (m_escaldo.has_value() && m_escaldou);
}
}
