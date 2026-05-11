#include <immediate_mode_vulkan/globals.h>

#include <algorithm>
#include <fstream>

#include <memory>
#include <nlohmann/json.hpp>
#include <string_view>

using namespace std;
using namespace nlohmann;

namespace imv {
    struct configuration_entry {
        json value;
    };
    bool create_missing = true, dirty = false;
    string file_name;
    map globals { make_unique<configuration_entry>() };

    void load_globals(std::string_view file_name) {
        imv::file_name = file_name;
        auto c = json::parse(ifstream(imv::file_name));
        auto v = c["test"];
        dirty = false;
    }

    void synchronize_globals() {
        if (dirty) {
            // TODO
            globals.entry->value.dump();
            dirty = false;
        } else {

        }
    }

    map map::operator[](const std::string_view& key) {
        auto& map = entry->value;
        // for reasons unknown to me, contains doesn't accept string_view in em
        std::string key_string(key);
        if (create_missing && !map.contains(key_string)) {
            map[key_string];
            dirty = true;
        }
        return { make_unique<configuration_entry>(map.at(key_string)) };
    }

    map map::operator[](std::size_t index) {
        auto& array = entry->value;
        if (create_missing && array.size() >= index) {
            array[index];
            dirty = true;
        }
        return { make_unique<configuration_entry>(array.at(index)) };
    }

    int deserialize<int>::operator()(map& value) {
        return value.entry->value;
    }

    float deserialize<float>::operator()(map& value) {
        return value.entry->value;
    }

    string_view deserialize<string_view>::operator()(map& value) {
        return (string&)value.entry->value;
    }
}
