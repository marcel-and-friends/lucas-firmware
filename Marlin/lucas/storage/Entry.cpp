#include "Entry.h"
#include <lucas/storage/sd/Card.h>

namespace lucas::storage {
std::optional<Entry> Entry::fetch(Id& id) {
    // TODO: initialize and retrieve nvm storage

    Entry result(id);
    result.m_file = sd::Card::the().open_file(result.m_id.name, O_RDWR | O_SYNC);
    if (result.m_file) {
        // empty file for some reason
        if (result.m_file->file_size() == 0) {
            purge(id);
            return std::nullopt;
        }
        return result;
    }

    return std::nullopt;
}

Entry Entry::fetch_or_create(Id& id) {
    Entry result(id);
    result.m_file = sd::Card::the().open_file(result.m_id.name, O_RDWR | O_SYNC | O_CREAT | O_TRUNC);
    return result;
}

void Entry::purge(Id& id) {
    sd::Card::the().delete_file(id.name);
}

}
