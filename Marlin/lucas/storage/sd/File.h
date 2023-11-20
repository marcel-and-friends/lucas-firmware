#pragma once

#include <src/sd/SdBaseFile.h>
#include <lucas/util/util.h>

namespace lucas::storage::sd {
class Card;

class File : SdBaseFile {
public:
    friend class Card;

    File() = default;

    File(File&& other) {
        *this = std::move(other);
    }

    File& operator=(File&& other) {
        SdBaseFile::operator=(other);
        // prevent the moved-from file from closing
        other.type_ = FAT_FILE_TYPE_CLOSED;
        return *this;
    }

    usize file_size() const {
        return fileSize();
    }

    void truncate(usize pos) {
        SdBaseFile::truncate(pos);
    }

    constexpr static auto MAX_ATTEMPTS = 3;

    template<typename T>
    bool write_binary(const T& buffer)
    requires requires { typename T::pointer; }
    {
        using Inner = std::remove_pointer_t<typename T::pointer>;

        for (auto attempts = 0; attempts <= MAX_ATTEMPTS; ++attempts) {
            if (write(buffer.data(), buffer.size() * sizeof(Inner)) != -1)
                return true;

            LOG_ERR("falha ao escrever parte de um arquivo, tentando novamente...");
        }
        return false;
    }

    bool write_binary(const auto& obj) {
        for (auto attempts = 0; attempts <= MAX_ATTEMPTS; ++attempts) {
            if (write(&obj, sizeof(obj)) != -1)
                return true;

            LOG_ERR("falha ao escrever parte de um arquivo, tentando novamente...");
        }
        return false;
    }

    template<typename T>
    bool read_binary_into(T& buffer)
    requires requires { typename T::pointer; }
    {
        using Inner = std::remove_pointer_t<typename T::pointer>;

        for (auto attempts = 0; attempts <= MAX_ATTEMPTS; ++attempts) {
            if (read(buffer.data(), buffer.size() * sizeof(Inner)) != -1)
                return true;

            LOG_ERR("falha ao ler parte de um arquivo, tentando novamente...");
        }
        return false;
    }

    bool read_binary_into(auto& obj) {
        for (auto attempts = 0; attempts <= MAX_ATTEMPTS; ++attempts) {
            if (read(&obj, sizeof(obj)) != -1)
                return true;

            LOG_ERR("falha ao ler parte de um arquivo, tentando novamente...");
        }
        return false;
    }
};
}
