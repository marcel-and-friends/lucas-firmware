#include "Receita.h"
#include <lucas/cmd/cmd.h>
#include <lucas/lucas.h>
#include <lucas/Fila.h>
#include <lucas/info/info.h>
#include <numeric>

namespace lucas {
JsonObjectConst Receita::padrao() {
    // calculado com o Assistant do ArduinoJson
    constexpr size_t TAMANHO_BUFFER = 384;
    static StaticJsonDocument<TAMANHO_BUFFER> doc;

    static char json[] = R"({
  "id": 61680,
  "tempoFinalizacao": 60000,
  "escaldo": {
    "duracao": 6000,
    "gcode": "L0 D9 N3 R1 T6000 G80"
  },
  "ataques": [
    {
      "duracao": 6000,
      "gcode": "L0 D7 N3 R1 T6000 G60",
      "intervalo": 24000
    },
    {
      "duracao": 9000,
      "gcode": "L0 D7 N5 R1 T9000 G90",
      "intervalo": 30000
    },
    {
      "duracao": 10000,
      "gcode": "L0 D7 N5 R1 T10000 G100",
      "intervalo": 35000
    },
    {
      "duracao": 9000,
      "gcode": "L0 D7 N5 R1 T9000 G100"
    }
  ]
})";

    static bool once = false;
    if (not once) {
        auto const err = deserializeJson(doc, json, sizeof(json));
        if (err) {
            LOG_ERR("DESSERIALIZACAO DA RECEITA PADRAO FALHOU - [", err.c_str(), "]");
            kill();
        }
        once = true;
    }

    return doc.as<JsonObjectConst>();
}

bool Receita::Passo::colide_com(Passo const& outro) const {
    // dois ataques colidem se:

    // 1. um comeca dentro do outro
    if (this->comeco_abs >= outro.comeco_abs and this->comeco_abs <= outro.fim())
        return true;

    // 2. um termina dentro do outro
    if (this->fim() >= outro.comeco_abs and this->fim() <= outro.fim())
        return true;

    // 3. a distancia entre o comeco de um e o fim do outro é menor que o tempo de viagem de uma estacao à outra
    if (this->comeco_abs > outro.fim() and this->comeco_abs - outro.fim() < util::MARGEM_DE_VIAGEM)
        return true;

    // 4. a distancia entre o fim de um e o comeco do outro é menor que o tempo de viagem de uma estacao à outra
    if (this->fim() < outro.comeco_abs and outro.comeco_abs - this->fim() < util::MARGEM_DE_VIAGEM)
        return true;

    return false;
}

void Receita::montar_com_json(JsonObjectConst json) {
    resetar();

    m_id = json["id"].as<size_t>();
    m_tempo_de_finalizacao = json["tempoFinalizacao"].as<millis_t>();

    // o escaldo é opcional
    if (json.containsKey("escaldo")) {
        auto escaldo_obj = json["escaldo"].as<JsonObjectConst>();
        auto& escaldo = m_passos[0];

        escaldo.duracao = escaldo_obj["duracao"].as<millis_t>();
        strcpy(escaldo.gcode, escaldo_obj["gcode"].as<char const*>());

        m_possui_escaldo = true;
    }

    auto ataques_obj = json["ataques"].as<JsonArrayConst>();
    for (size_t i = 0; i < ataques_obj.size(); ++i) {
        auto ataque_obj = ataques_obj[i];
        auto& ataque = m_passos[i + m_possui_escaldo];

        ataque.duracao = ataque_obj["duracao"].as<millis_t>();
        ataque.intervalo = ataque_obj.containsKey("intervalo") ? ataque_obj["intervalo"].as<millis_t>() : 0;
        strcpy(ataque.gcode, ataque_obj["gcode"].as<char const*>());
    }

    m_num_passos = ataques_obj.size() + m_possui_escaldo;
}

void Receita::resetar() {
    m_id = 0;

    m_possui_escaldo = false;

    m_tempo_de_finalizacao = 0;
    m_inicio_tempo_finalizacao = 0;

    m_num_passos = 0;
    m_passos = {};

    m_passo_atual = 0;
}

void Receita::executar_passo_atual() {
    GcodeSuite::process_subcommands_now(F(m_passos[m_passo_atual].gcode));
    // é muito importante esse número só incrementar APÓS a execucão do passo
    m_passo_atual++;
}

void Receita::mapear_passos_pendentes(millis_t tick) {
    for_each_passo_pendente([&](Passo& passo) {
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
}
