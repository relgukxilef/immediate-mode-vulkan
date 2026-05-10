#include <string_view>
#include <unordered_map>
#include <string>
#include <memory>

namespace imv {
    void load_globals(std::string_view file_name);

    void synchronize_globals();

    template<class T> struct deserialize;

    struct configuration_entry;

    struct map {
        map operator[](const std::string_view& key);
        // string_view look-up is only supported with C++26
        map operator[](std::size_t index);

        template<class T> operator T();

        std::unique_ptr<configuration_entry> entry;
    } extern globals;

    template<class T> map::operator T() {
        return deserialize<T>()(*this);
    }

    template<> struct deserialize<int> {
        int operator()(map& value);
    };

    template<> struct deserialize<float> {
        float operator()(map& value);
    };

    template<> struct deserialize<std::string_view> {
        std::string_view operator()(map& value);
    };

    template<> struct deserialize<std::string> {
        std::string operator()(map& value) {
            return std::string(deserialize<std::string_view>()(value));
        }
    };
}
