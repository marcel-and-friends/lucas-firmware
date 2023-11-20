#include "storage.h"
#include <lucas/storage/sd/Card.h>

namespace lucas::storage {
static std::array<Entry::Id, 10> s_entry_identifiers{};
static usize s_current_entry = 0;

void setup() {
    sd::Card::the().setup();
}

void tick() {
    sd::Card::the().tick();
}

Handle register_handle_for_entry(const char* name, usize size) {
    if (s_current_entry == s_entry_identifiers.size()) {
        LOG_ERR("nao tem mais espaco para registrar entradas");
        kill();
    }

    auto& id = s_entry_identifiers[s_current_entry] = Entry::Id{ name, size };
    LOG("registering entry ", id.name, " - ", s_current_entry);
    return s_current_entry++;
}

void purge_entry(Handle handle) {
    auto& id = s_entry_identifiers[handle];
    Entry::purge(id);
    LOG("purged entry \"", id.name, "\"");
}

std::optional<Entry> fetch_entry(Handle handle) {
    return Entry::fetch(s_entry_identifiers[handle]);
}

Entry fetch_or_create_entry(Handle handle) {
    return Entry::fetch_or_create(s_entry_identifiers[handle]);
}
}

/*
MKS Robin Nano address
BootLoader：0x08000000-0x0800C000
App start address：0x0800C000
*/
