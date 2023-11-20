#pragma once

#include <lucas/storage/Entry.h>
#include <lucas/types.h>

namespace lucas::storage {
using Handle = usize;

void setup();

void tick();

Handle register_handle_for_entry(const char* name, usize size);

void purge_entry(Handle);

std::optional<Entry> fetch_entry(Handle);

Entry fetch_or_create_entry(Handle);

template<typename T>
T create_or_update_entry(Handle handle, std::optional<T> optional, T fallback) {
    T result;
    if (optional) {
        result = *optional;
    } else {
        auto entry = fetch_entry(handle);
        result = entry ? entry->read_binary<T>() : fallback;
    }

    auto entry = fetch_or_create_entry(handle);
    entry.write_binary(result);
    return result;
}

// this exists purely to make certain callsites more readable
// example: cfg::setup()
inline Entry create_entry(Handle handle) {
    return fetch_or_create_entry(handle);
}
}
