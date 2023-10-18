#pragma once

#include <src/sd/cardreader.h>
#include <lucas/sec/sec.h>
#include <lucas/util/Singleton.h>
#include <string_view>
#include <span>

namespace lucas::util {
class SD {
public:
    static SD make() {
        return {};
    }

    ~SD() {
        if (card.isFileOpen())
            card.closefile();
    }

    enum class OpenMode {
        Read,
        Write
    };

    bool open(std::string_view file_path, OpenMode mode) {
        if (mode == OpenMode::Read) {
            card.openFileRead(file_path.data());
        } else if (mode == OpenMode::Write) {
            card.openFileWrite(file_path.data());
        }
        return card.isFileOpen();
    }

    bool file_exists(std::string_view file_path) {
        return card.fileExists(file_path.data());
    }

    void remove_file(std::string_view file_path) {
        return card.removeFile(file_path.data());
    }

    bool is_open() {
        return card.isFileOpen();
    }

    void close() {
        card.closefile();
    }

    template<typename T>
    bool write_from(const T& buffer)
    requires requires { typename T::pointer; }
    {
        using Inner = std::remove_pointer_t<typename T::pointer>;
        return card.write(buffer.data(), buffer.size() * sizeof(Inner)) != -1;
    }

    template<typename T>
    bool write_from(const T& obj) {
        return card.write(&obj, sizeof(obj)) != -1;
    }

    template<typename T>
    bool read_into(T& buffer)
    requires requires { typename T::pointer; }
    {
        using Inner = std::remove_pointer_t<typename T::pointer>;
        return card.read(buffer.data(), buffer.size() * sizeof(Inner)) != -1;
    }

    template<typename T>
    bool read_into(T& obj) {
        return card.read(&obj, sizeof(obj)) != -1;
    }
};
}
