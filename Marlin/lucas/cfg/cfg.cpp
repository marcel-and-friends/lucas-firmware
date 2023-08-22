#include <lucas/cfg/cfg.h>
#include <lucas/Spout.h>
#include <lucas/mem/FlashReader.h>
#include <lucas/mem/FlashWriter.h>

namespace lucas::cfg {
constexpr auto DEFAULT_OPTIONS = std::to_array<Option>({
    [LogPour] = {.id = 'D',  .active = true },
    [LogTravel] = { .id = 'V', .active = true },
    [LogQueue] = { .id = 'F', .active = true },
    [LogCalibration] = { .id = 'N', .active = true },
    [LogStations] = { .id = 'E', .active = true },
    [LogInfo] = { .id = 'I', .active = true },

    [LogSerial] = { .id = 'S', .active = false},
    [LogWifi] = { .id = 'W', .active = false},
    [LogGcode] = { .id = 'G', .active = false},
    [LogLn] = { .id = 'L', .active = false},

    [GigaMode] = { .id = 'M', .active = false},

    [SetTargetTemperatureOnCalibration] = { .id = 'T', .active = true },
    [FillDigitalSignalTableOnCalibration] = { .id = 'X', .active = false},
});

consteval bool doesnt_have_duplicated_ids(const OptionList& opcoes) {
    for (size_t i = 1; i < opcoes.size(); ++i)
        for (size_t j = 0; j < i; ++j)
            if (opcoes[i].id != Option::ID_DEFAULT and opcoes[j].id != Option::ID_DEFAULT)
                if (opcoes[i].id == opcoes[j].id)
                    return false;

    return true;
}

static_assert(DEFAULT_OPTIONS.size() == Options::Count, "tamanho errado irmao");
static_assert(doesnt_have_duplicated_ids(DEFAULT_OPTIONS), "opcoes duplicadas irmao");

// vai ser propriamente inicializado na 'setup()'
static OptionList s_options = DEFAULT_OPTIONS;

constexpr auto CFG_FLASH_ADDRESS_START = sizeof(Spout::FlowController::Table);
constexpr auto OPTION_SIZE = sizeof(char) + sizeof(bool);

static void fetch_options_from_flash();

void setup() {
    auto reader = mem::FlashReader(CFG_FLASH_ADDRESS_START);
    const auto first_saved_id = reader.read<char>(CFG_FLASH_ADDRESS_START);
    if (first_saved_id != DEFAULT_OPTIONS[0].id) {
        LOG("cfg ainda nao foi salva na flash, usando valores padroes");
        s_options = DEFAULT_OPTIONS;
        save_options_to_flash();
    } else {
        fetch_options_from_flash();
    }
}

void save_options_to_flash() {
    auto writer = mem::FlashWriter(CFG_FLASH_ADDRESS_START);
    for (size_t i = 0; i < s_options.size(); ++i) {
        const auto& option = s_options[i];
        const auto offset = i * OPTION_SIZE;
        writer.write<char>(offset, option.id);
        writer.write<bool>(offset + sizeof(char), option.active);
    }

    LOG("opcoes escritas na flash");
}

static void fetch_options_from_flash() {
    auto reader = mem::FlashReader(CFG_FLASH_ADDRESS_START);
    for (size_t i = 0; i < s_options.size(); ++i) {
        auto& option = s_options[i];
        const auto offset = i * OPTION_SIZE;
        option.id = reader.read<char>(offset);
        option.active = reader.read<bool>(offset + sizeof(char));
    }

    LOG("opcoes lidas da flash");
}

void reset_options() {
    s_options = DEFAULT_OPTIONS;
    auto writer = mem::FlashWriter(CFG_FLASH_ADDRESS_START);
    for (size_t i = 0; i < s_options.size(); ++i) {
        const auto offset = i * OPTION_SIZE;
        writer.write<char>(offset, Option::ID_DEFAULT);
        writer.write<bool>(offset + sizeof(char), false);
    }
}

Option get(Options option) {
    return s_options[size_t(option)];
}

OptionList& opcoes() {
    return s_options;
}
}
