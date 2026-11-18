#pragma once

#include <string_view>
#include <string>
#include <memory>

namespace imv {
    struct property_value;
    struct scene_data;

    struct property {
        ~property();

        property operator[](std::string_view key);
        property operator[](std::size_t index);

        template<class T> operator T();

        std::shared_ptr<property_value> value;
    };

    struct scene : property {
        scene();
        scene(std::string filename, bool create_missing = true);
        
        ~scene();

        void synchronize();

        std::shared_ptr<scene_data> data;
        std::string filename;
        bool create_missing = true, dirty = false;
    };

    template<class T> struct deserialize;

    template<class T> property::operator T() {
        return deserialize<T>()(*this);
    }

    template<> struct deserialize<int> {
        int operator()(property& value);
    };

    template<> struct deserialize<float> {
        float operator()(property& value);
    };

    template<> struct deserialize<std::string_view> {
        std::string_view operator()(property& value);
    };

    template<> struct deserialize<std::string> {
        std::string operator()(property& value) {
            return std::string(deserialize<std::string_view>()(value));
        }
    };
}
