#pragma once

namespace lucas::util {
template<typename T>
class Singleton {
public:
    static T& the() {
        static T the;
        return the;
    }

protected:
    Singleton() = default;

public:
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    Singleton(Singleton&&) = delete;
    Singleton& operator=(Singleton&&) = delete;
};
}
