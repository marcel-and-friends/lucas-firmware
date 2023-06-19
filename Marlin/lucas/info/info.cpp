#include "info.h"
#include <lucas/lucas.h>
#include <lucas/Estacao.h>
#include <lucas/cmd/cmd.h>
#include <lucas/serial/serial.h>
#include <lucas/Fila.h>

namespace lucas::info {

void tick() {
    // gostaria de nao alocar 2 bagui de 1kb mas nao sei komo
    DocumentoJson doc;
    char output[BUFFER_SIZE] = {};
    bool atualizou = false;
    Report::for_each([&](Report& report) {
        const auto tick = millis();
        const auto delta = report.delta(tick);
        if (delta < report.intervalo || !report.callback)
            return util::Iter::Continue;

        if (report.condicao && !report.condicao())
            return util::Iter::Continue;

        report.callback(tick, doc.createNestedObject(report.nome));
        report.ultimo_tick_reportado = tick;
        atualizou = true;

        return util::Iter::Continue;
    });

    if (atualizou) {
        serializeJson(doc, output);
        LOG("json:\n", output);
    }
}

void interpretar_json(std::span<char> buffer) {
    DocumentoJson doc;
    const auto err = deserializeJson(doc, buffer.data(), buffer.size());
    if (err) {
        LOG("desserializacao json falhou - [", err.c_str(), "]");
        return;
    }

    const auto root = doc.as<JsonObjectConst>();
    for (const auto obj : root) {
        const auto chave = obj.key();
        if (chave.size() > 1) {
            LOG("chave invalida - [", chave.c_str(), "]");
            continue;
        }

        const auto opcao = static_cast<size_t>(*chave.c_str() - '0');
        const auto v = obj.value();
        switch (opcao) {
        case 0: { // inicializar estacoes
            if (!v.is<size_t>()) {
                LOG("valor json invalido para numero de estacoes");
                break;
            }

            Estacao::iniciar(v.as<size_t>());
        } break;
        case 1: { // temperatura target do boiler
            if (!v.is<const char*>()) {
                LOG("valor json invalido para temperatura target do boiler");
                break;
            }

            cmd::executar_fmt("M140 S%s", v.as<const char*>());
        } break;
        case 2: { // estacoes bloqueadas
            if (!v.is<JsonArrayConst>()) {
                LOG("valor json invalido para bloqueamento de bocas");
                break;
            }

            auto array = v.as<JsonArrayConst>();
            if (array.isNull() || array.size() != Estacao::num_estacoes()) {
                LOG("array de estacoes bloqueadas invalido");
                break;
            }

            Estacao::for_each([&array](Estacao& estacao) {
                estacao.set_bloqueada(array[estacao.index()].as<bool>());
                return util::Iter::Continue;
            });
        } break;
        case 3: { // cancelar receita da estacao
            if (!v.is<size_t>()) {
                LOG("valor json invalido para cancelamento de receita");
                break;
            }

            auto index = v.as<size_t>();
            if (index >= Estacao::num_estacoes()) {
                LOG("index para cancelamento de receita invalido");
                break;
            }

            Fila::the().cancelar_receita(index);
        } break;
        case 4: { // enviar uma receita para uma estacao
            const auto obj = v.as<JsonObjectConst>();
            const auto estacao = obj["estacao"].as<size_t>();
            Fila::the().agendar_receita(estacao, Receita::from_json(obj));
        } break;
        case 5: { // enviar a receita padrao para uma ou mais estacoes
            if (v.is<size_t>()) {
                Fila::the().agendar_receita(v.as<size_t>(), Receita::padrao());
            } else if (v.is<JsonArrayConst>()) {
                for (auto idx : v.as<JsonArrayConst>()) {
                    Fila::the().agendar_receita(idx.as<size_t>(), Receita::padrao());
                }
            }
        } break;
        case 6: {
            if (v.is<size_t>()) {
                Fila::the().mapear_receita(v.as<size_t>());
            } else if (v.is<JsonArrayConst>()) {
                for (auto idx : v.as<JsonArrayConst>()) {
                    Fila::the().mapear_receita(idx.as<size_t>());
                }
            }
        } break;
        default:
            LOG("opcao invalida - [", opcao, "]");
        }
    }
}
}
