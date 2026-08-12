#include "nlohmann/detail/json_pointer.hpp"
#include <immediate_mode_vulkan/globals.h>

#include <fstream>

#include <nlohmann/json.hpp>

#include <memory>
#include <string_view>
#include <vector>
#include <unordered_map>

using namespace std;
using namespace nlohmann;

namespace imv {

    struct scene_data {
        json file;
    };

    struct property_value {
        weak_ptr<property_value> parent;
        weak_ptr<scene_data> root;
        string key;
        unsigned index;
        string text;
        int number;
        float decimal;
        vector<shared_ptr<property_value>> array;
        unordered_map<string, shared_ptr<property_value>> object;
        // TODO: store additional semantic information here
    };

    property::~property() = default;

    scene::scene() = default;

    scene::scene(string filename, bool create_missing) : 
        create_missing(create_missing) 
    {
        data = make_shared<scene_data>();
        value = make_shared<property_value>(property_value{{}, data});
        try {
            data->file = json::parse(std::ifstream(filename.c_str()));
            // TODO: parse into property_value objects
        } catch(const json::parse_error&) {
            if (!create_missing)
                throw;
            // TODO: create file
        }
        this->filename = std::move(filename);
    }

    scene::~scene() = default;

    property property::operator[](string_view key) {
        auto &field = value->object[string(key)];
        if (!field)
            field = make_shared<property_value>(value, value->root);
        return {field};
    }

    property property::operator[](size_t key) {
        if (value->array.size() <= key)
            value->array.resize(key + 1);
        auto &field = value->array[key];
        if (!field)
            field = make_shared<property_value>(value, value->root);
        return {field};
    }

    int deserialize<int>::operator()(property& value) {
        // TODO: create value if it doesn't exist
        return value.value->number;
    }

    float deserialize<float>::operator()(property& value) {
        return value.value->decimal;
    }

    string_view deserialize<string_view>::operator()(property& value) {
        return value.value->text;
    }
}
