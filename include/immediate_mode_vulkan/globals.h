#include <string_view>
#include <unordered_map>
#include <string>

namespace imv {
    void load_globals(std::string_view file_name);

    template<class T> struct deserialize;

    struct configuration_entry {
        std::unordered_map<std::string, configuration_entry> map;
        std::vector<configuration_entry> array;
        std::string text;
        float number;
    };

    struct map {
        map operator[](const std::string& key);
        // string_view look-up is only supported with C++26
        map operator[](std::size_t index);

        template<class T> operator T();

        configuration_entry* entry;
    } extern globals;

    template<class T> map::operator T() {
        return deserialize<T>()(*this);
    }

    template<> struct deserialize<int> {
        int operator()(map& value) {
            return (int)value.entry->number;
        }
    };

    template<> struct deserialize<float> {
        float operator()(map& value) {
            return value.entry->number;
        }
    };

    template<> struct deserialize<std::string> {
        std::string operator()(map& value) {
            return value.entry->text;
        }
    };

    template<> struct deserialize<std::string_view> {
        std::string_view operator()(map& value) {
            return value.entry->text;
        }
    };
}
