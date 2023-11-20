#include "Card.h"
#include <lucas/util/util.h>
#include <memory>

namespace lucas::storage::sd {
void Card::setup() {
    tick();
}

void Card::tick() {
    const auto state = insertion_state();
    if (state == m_last_state)
        return;

    m_last_state = state;
    if (state == InsertionState::Inserted) {
        mount();
    } else {
        LOG("cartao SD removido");
        m_mounted = false;
    }
}

void Card::mount() {
    m_mounted = false;
    if (m_root.isOpen())
        m_root.close();

    if (not m_driver.init(SD_SPI_SPEED, SDSS))
        return;

    if (not m_volume.init(&m_driver))
        return;

    if (not m_root.openRoot(&m_volume))
        return;

    m_mounted = true;
    LOG("cartao SD montado");
}

std::optional<File> Card::open_file(const char* path, usize flags) {
    if (not m_mounted)
        return std::nullopt;

    File file;
    if (not file.open(&m_root, path, flags))
        return std::nullopt;

    return std::move(file);
}

void Card::delete_file(const char* path) {
    if (not m_mounted)
        // TODO?: schedule this operation for later?
        return;

    File::remove(&m_root, path);
}

Card::InsertionState Card::insertion_state() const {
    return digitalRead(SD_DETECT_PIN) == SD_DETECT_STATE ? InsertionState::Inserted : InsertionState::Removed;
}
}
