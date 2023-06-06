#include "lucas.h"
#include <lucas/Estacao.h>
#include <lucas/serial/HookDelimitado.h>
#include <ArduinoJson.h>

namespace lucas {
void setup() {
    LOG("iniciando lucas");

    // clang-format off
    static constexpr auto options_handler = std::to_array<void (*)(JsonVariantConst)>({
        [](JsonVariantConst v) { // 0 - numero de estacoes
            if (!v.is<size_t>()) {
                LOG("valor json invalido para numero de estacoes");
                return;
            }

            Estacao::iniciar(v.as<size_t>());
        },
        [](JsonVariantConst v) { // 1 - temperatura target do boiler
            if (!v.is<const char*>()) {
                LOG("valor json invalido para temperatura target do boiler");
                return;
            }

            gcode::executar_fmt("M140 S%s", v.as<const char*>());
        },
        [](JsonVariantConst v) { // 2 - estacoes bloqueadas
            if (!v.is<JsonArrayConst>()) {
                LOG("valor json invalido para bloqueamento de bocas");
                return;
            }

            auto array = v.as<JsonArrayConst>();
            if (array.isNull() || array.size() != Estacao::num_estacoes()) {
                LOG("array de estacoes bloqueadas invalido");
                return;
            }

            Estacao::for_each([&array](Estacao& estacao) {
                estacao.set_bloqueada(array[estacao.index()].as<bool>());
                return util::Iter::Continue;
            });
        },
        [](JsonVariantConst v) { // 3 - cancelar receita da estacao
            if (!v.is<size_t>()) {
                LOG("valor json invalido para cancelamento de receita");
                return;
            }

            auto index = v.as<size_t>();
            if (index >= Estacao::num_estacoes()) {
                LOG("index para cancelamento de receita invalido");
                return;
            }

            Estacao::lista().at(index).cancelar_receita();
        }
    });
    // clang-format on

    serial::HookDelimitado::make('#', [](std::span<char> buffer) {
        StaticJsonDocument<serial::HookDelimitado::BUFFER_SIZE> doc;
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
            if (opcao >= options_handler.size()) {
                LOG("opcao invalida - [", opcao, "]");
                continue;
            }

            options_handler[opcao](obj.value());
        }
    });

    Bico::the().setup();
#if LUCAS_CONECTAR_WIFI
    wifi::conectar(LUCAS_WIFI_NOME_SENHA);
#endif
}

void pos_execucao_gcode() {
    if (wifi::conectando()) {
        if (wifi::terminou_de_conectar())
            wifi::informar_sobre_rede();
        return;
    }

    if (!Estacao::ativa())
        return;

    Estacao::ativa()->prosseguir_receita();
}
}
