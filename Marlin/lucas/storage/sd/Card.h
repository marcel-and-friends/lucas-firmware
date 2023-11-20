#pragma once

#include <lucas/types.h>
#include <lucas/storage/sd/File.h>
#include <lucas/util/Singleton.h>
#include <src/sd/SdFatConfig.h>
#include <src/sd/SdInfo.h>
#include <src/sd/disk_io_driver.h>
#include <optional>

namespace lucas::storage::sd {
class Card : public util::Singleton<Card> {
public:
    void setup();

    void tick();

    std::optional<File> open_file(const char* path, usize flags);

    void delete_file(const char* path);

    bool is_mounted() const { return m_mounted; }

private:
    void mount();

    enum class InsertionState {
        Inserted,
        Removed,
        Unknown
    };

    InsertionState insertion_state() const;

    DiskIODriver_SPI_SD m_driver;

    SdVolume m_volume;

    SdBaseFile m_root;

    InsertionState m_last_state = InsertionState::Unknown;

    bool m_mounted = false;
};
}
