#include <immediate_mode_vulkan/globals.h>

#include <algorithm>
#include <fstream>

using namespace std;

namespace imv {
    configuration_entry configuration;
    bool create_missing = true;
    map globals { &configuration };

    void load_globals(std::string_view file_name) {
        ifstream file(string{file_name});
        vector<configuration_entry*> stack;
        while (file.is_open()) {
            char c = file.get();
            if (c == '}') {
                stack.pop_back();
            }
            // TODO
        }
    }

    map map::operator[](const std::string& key) {
        auto& map = entry->map;
        if (create_missing)
            map[key];
        return { &map.at(key) };
    }

    map map::operator[](std::size_t index) {
        auto& array = entry->array;
        if (create_missing)
            array.resize(std::max(array.size(), index + 1));
        return { &array.at(index) };
    }
}
