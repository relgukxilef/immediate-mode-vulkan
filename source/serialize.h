#include <span>
#include <type_traits>
#include <vector>
#include <concepts>

#include <vulkan/vulkan.h>

namespace imv {
    template<class T>
    void update_hash(size_t& hash, T value) {
        hash = (hash * 820541279138450587ull) ^ std::hash<T>()(value);
    }

    template<class T>
    struct tag_t {};
    
    template<std::integral T>
    void visit(size_t& hash, auto value, tag_t<T>) {
        update_hash(hash, value);
    }

    template<class T>
    std::enable_if_t<std::is_enum_v<T>> visit(
        size_t& hash, auto value, tag_t<T>
    ) {
        update_hash(hash, value);
    }

    void visit(auto&& visitor, auto&& object) {
        visit(visitor, object, tag_t<std::remove_cvref_t<decltype(object)>>());
    }

    template<class C>
    void visit(auto&& visitor, auto object, tag_t<std::span<C>>) {
        visit(visitor, std::size(object));
        for (auto& item : object) {
            visit(visitor, item, tag_t<std::remove_cvref_t<C>>());
        }
    }

    template<class C>
    void visit(auto&& visitor, auto&& object, tag_t<std::vector<C>>) {
        visit(visitor, std::span<const C>(object));
    }

    void visit_array(auto&& visitor, auto* pointer, size_t size) {
        visit(visitor, std::span<decltype(*pointer)>(pointer, size));
    }
    
    void visit(auto&& visitor, auto&& object, tag_t<VkDescriptorPoolSize>) {
        visit(visitor, object.type);
        visit(visitor, object.descriptorCount);
    }

    struct hasher {
        size_t operator()(auto&& object) const {
            size_t result = 0;
            visit(result, object);
            return result;
        }
    };

}

inline bool operator==(
    const VkDescriptorPoolSize& a, const VkDescriptorPoolSize& b
) {
    // TODO: use exact comparison
    return imv::hasher()(a) == imv::hasher()(b);
}
