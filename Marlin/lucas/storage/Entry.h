#pragma once

#include <lucas/storage/sd/File.h>
#include <lucas/types.h>
#include <optional>

namespace lucas::storage {
class Entry {
public:
    struct Id {
        const char* name = nullptr;
        usize size = 0;
    };

    static std::optional<Entry> fetch(Id);
    static Entry fetch_or_create(Id);

    static void purge(Id);
    void purge() { purge(m_id); }

    // TODO: write a `storage::Buffer` class that takes ranges or objects and gives you a data() ptr and size()
    // then use that to check the buffer size vs m_size on {write|read}_binary
    void write_binary(const auto& buffer) {
        if (m_file) {
            if (m_file->write_binary(buffer))
                return;
            // if we fail mark the file as invalid
            m_file.reset();
        }
    }

    void read_binary_into(auto& buffer) {
        if (m_file) {
            if (m_file->read_binary_into(buffer))
                return;
            // if we fail mark the file as invalid
            m_file.reset();
        }
    }

    template<typename T>
    T read_binary() {
        T result;
        read_binary_into(result);
        return result;
    }

private:
    Entry(Id id)
        : m_id(id) {
    }

    Id m_id;

    std::optional<sd::File> m_file;
    // TODO:
    // nvm::Blob m_blob;
};
}
