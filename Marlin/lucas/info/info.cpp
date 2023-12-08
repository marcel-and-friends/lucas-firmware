#include "info.h"
#include <lucas/lucas.h>
#include <lucas/Station.h>
#include <lucas/RecipeQueue.h>
#include <lucas/Spout.h>
#include <lucas/Boiler.h>
#include <lucas/core/core.h>
#include <lucas/cmd/cmd.h>
#include <lucas/sec/sec.h>
#include <lucas/serial/serial.h>

namespace lucas::info {
void tick() {
    if (CFG(LogTemperatureForTesting)) {
        every(1s) {
            LOG("TEMPERATURA: ", Boiler::the().temperature());
        }
    }

    every(1s) {
        static u32 s_stored_pulse_counter_for_logging = 0;
        if (CFG(LogFlowSensorDataForTesting) and s_stored_pulse_counter_for_logging != Spout::s_pulse_counter) {
            LOG("SENSOR_FLUXO: ", Spout::s_pulse_counter, " - ", Spout::s_pulse_counter - s_stored_pulse_counter_for_logging);
            s_stored_pulse_counter_for_logging = Spout::s_pulse_counter;
        }
    }

    JsonDocument doc;
    bool updated = false;
    Report::for_each([&](Report& report) {
        const auto tick = millis();
        const auto delta = report.delta(tick);
        if (delta < report.interval or not report.callback)
            return util::Iter::Continue;

        if (report.condition and not report.condition())
            return util::Iter::Continue;

        report.callback(doc.createNestedObject(report.nome));
        report.last_reported_tick = tick;
        updated = true;

        return util::Iter::Continue;
    });

    if (updated)
        print_json(doc);
}

// https://www.notion.so/Comandos-enviados-do-app-para-a-m-quina-683dd32fcf93481bbe72d6ca276e7bfb?pvs=4
static Command command_from_string(std::string_view cmd) {
    static constexpr auto map = std::to_array({
        [usize(Command::RequestInfoCalibration)] = "reqInfoCalibration"sv,
        [usize(Command::InitializeStations)] = "cmdInitializeStations"sv,
        [usize(Command::SetBoilerTemperature)] = "cmdSetBoilerTemperature"sv,
        [usize(Command::ScheduleRecipe)] = "cmdScheduleRecipe"sv,
        [usize(Command::CancelRecipe)] = "cmdCancelRecipe"sv,
        [usize(Command::RequestInfoAllStations)] = "reqInfoAllStations"sv,
        [usize(Command::FirmwareUpdate)] = "cmdFirmwareUpdate"sv,
        [usize(Command::RequestInfoFirmware)] = "reqInfoFirmware"sv,
        [usize(Command::SetFixedRecipes)] = "cmdSetFixedRecipes"sv,
        [usize(Command::DevScheduleStandardRecipe)] = "devScheduleStandardRecipe"sv,
        [usize(Command::DevSimulateButtonPress)] = "devSimulateButtonPress"sv,
    });

    auto it = std::find(map.begin(), map.end(), cmd);
    return it == map.end() ? Command::InvalidCommand : Command(std::distance(map.begin(), it));
}

static std::array<CommandHook, usize(Command::Count)> s_command_hooks = {};

void install_command_hook(Command command, CommandHook hook) {
    s_command_hooks[usize(command)] = hook;
}

void uninstall_command_hook(Command command) {
    s_command_hooks[usize(command)] = nullptr;
}

info::TemporaryCommandHook::TemporaryCommandHook(Command command, CommandHook hook)
    : m_command(command)
    , m_old_hook(s_command_hooks[usize(command)]) {
    install_command_hook(command, hook);
}

info::TemporaryCommandHook::~TemporaryCommandHook() {
    install_command_hook(m_command, m_old_hook);
}

void interpret_command_from_host(std::span<char> buffer) {
    JsonDocument doc;
    const auto err = deserializeJson(doc, buffer.data(), buffer.size());
    if (err) {
        LOG_ERR("desserializacao json falhou - [", err.c_str(), "]");
        return;
    }

    const auto root = doc.as<JsonObjectConst>();
    for (const auto obj : root) {
        if (obj.key().isNull() or obj.key().size() == 0) {
            LOG_ERR("chave invalida");
            continue;
        }

        const auto command = command_from_string(obj.key().c_str());
        const auto v = obj.value();

        if (auto hook = s_command_hooks[usize(command)]) {
            hook();
            continue;
        }

        switch (command) {
        case Command::InvalidCommand: {
            LOG_ERR("comando invalido");
        } break;
        case Command::RequestInfoCalibration: {
            core::inform_calibration_status();
        } break;
        case Command::InitializeStations: {
            if (not v.is<JsonArrayConst>()) {
                LOG_ERR("valor json invalido para inicializar estacoes");
                break;
            }

            auto array = v.as<JsonArrayConst>();
            Station::SharedData<bool> blocked_stations = {};
            for (usize i = 0; i < array.size(); ++i)
                blocked_stations[i] = not array[i].as<bool>();

            Station::initialize(array.size(), blocked_stations);
        } break;
        case Command::SetBoilerTemperature: {
            if (not v.is<s32>()) {
                LOG_ERR("valor json invalido para temperatura target do boiler");
                break;
            }

            core::calibrate(v.as<s32>());
        } break;
        case Command::ScheduleRecipe: {
            if (not v.is<JsonObjectConst>()) {
                LOG_ERR("valor json invalido para envio de uma receita");
                break;
            }

            RecipeQueue::the().schedule_recipe(v.as<JsonObjectConst>());
        } break;
        case Command::CancelRecipe: {
            if (not v.is<usize>() and not v.is<JsonArrayConst>()) {
                LOG_ERR("valor json invalido para cancelamento de receita");
                break;
            }

            if (v.is<usize>()) {
                auto index = v.as<usize>();
                if (index >= Station::number_of_stations()) {
                    LOG_ERR("index para cancelamento de receita invalido");
                    break;
                }
                RecipeQueue::the().cancel_station_recipe(index);
            } else if (v.is<JsonArrayConst>()) {
                for (auto index : v.as<JsonArrayConst>()) {
                    RecipeQueue::the().cancel_station_recipe(index.as<usize>());
                }
            }
        } break;
        case Command::RequestInfoAllStations: {
            if (not v.is<JsonArrayConst>()) {
                LOG_ERR("valor json invalido para requisicao de informacoes");
                break;
            }
            const auto stations = v.as<JsonArrayConst>();
            if (stations.size() > Station::number_of_stations()) {
                LOG_ERR("lista de requisicao muito grande - [size = ", stations.size(), " - max = ", Station::number_of_stations(), "]");
                return;
            }
            RecipeQueue::the().send_queue_info(stations);
        } break;
        case Command::FirmwareUpdate: {
            if (not v.is<usize>()) {
                LOG_ERR("valor json invalido para tamanho do firmware novo");
                break;
            }
            core::prepare_for_firmware_update(v.as<usize>());
        } break;
        case Command::RequestInfoFirmware: {
            info::send(
                info::Event::Firmware,
                [](JsonObject o) {
                    o["version"] = cfg::FIRMWARE_VERSION;
                });
        } break;
        case Command::SetFixedRecipes: {
            if (not v.is<JsonObjectConst>()) {
                LOG_ERR("valor json invalido para envio de uma receita fixa");
                break;
            }

            RecipeQueue::the().set_fixed_recipes(v.as<JsonObjectConst>());
        } break;
        /* ~comandos de desenvolvimento~ */
        case Command::DevScheduleStandardRecipe: {
            if (not v.is<usize>()) {
                LOG_ERR("valor json invalido para envio da receita standard");
                break;
            }

            for (usize i = 0; i < v.as<usize>(); ++i)
                RecipeQueue::the().schedule_recipe(Recipe::standard());

        } break;
        case Command::DevSimulateButtonPress: {
            if (not v.is<usize>() and not v.is<JsonArrayConst>()) {
                LOG_ERR("valor json invalido para apertar button");
                break;
            }

            auto simulate_button_press = [](usize index) {
                auto& station = Station::list().at(index);
                if (station.status() == Station::Status::Ready) {
                    station.set_status(Station::Status::Free);
                } else {
                    RecipeQueue::the().map_station_recipe(index);
                }
            };

            if (v.is<usize>()) {
                simulate_button_press(v.as<usize>());
            } else if (v.is<JsonArrayConst>()) {
                for (auto index : v.as<JsonArrayConst>()) {
                    simulate_button_press(index);
                }
            }
        }
        }
    }
}

void print_json(const JsonDocument& doc) {
    SERIAL_CHAR('#');
    serializeJson(doc, SERIAL_IMPL);
    SERIAL_ECHOLNPGM("#");
}
}
