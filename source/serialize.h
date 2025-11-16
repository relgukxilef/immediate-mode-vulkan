#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>
#include <concepts>

#include <vulkan/vulkan.h>

namespace imv {
    template<class T>
    struct tag_t {};
    
    template<std::integral T>
    void visit(std::vector<uint64_t>& buffer, auto value, tag_t<T>) {
        buffer.push_back(uint64_t(value));
    }

    template<class T>
    std::enable_if_t<std::is_enum_v<T>> visit(
        std::vector<uint64_t>& buffer, auto value, tag_t<T>
    ) {
        buffer.push_back(uint64_t(value));
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

    template<class T>
    void visit_array(auto&& visitor, T* pointer, size_t size) {
        visit(visitor, std::span<T>(pointer, size));
    }
    

    void visit(auto&& visitor, auto&& object, tag_t<VkDescriptorPoolSize>) {
        visit(visitor, object.type);
        visit(visitor, object.descriptorCount);
    }

    void visit(
        auto&& visitor, auto&& object, tag_t<VkDescriptorPoolCreateInfo>
    ) {
        visit(visitor, object.sType);
        //visit(visitor, object.pNext);
        visit(visitor, object.flags);
        visit(visitor, object.maxSets);
        visit_array(visitor, object.pPoolSizes, object.poolSizeCount);
    }

    void visit(
        auto&& visitor, auto&& object, tag_t<VkDescriptorSetLayoutBinding>
    ) {
        visit(visitor, object.binding);
        visit(visitor, object.descriptorType);
        visit(visitor, object.descriptorCount);
        visit(visitor, object.stageFlags);
        // TODO: object.pImmutableSamplers is a handle
        visit(visitor, object.descriptorCount);
    }

    void visit(
        auto&& visitor, auto&& object, tag_t<VkDescriptorSetLayoutCreateInfo>
    ) {
        visit(visitor, object.sType);
        //visit(visitor, object.pNext);
        visit(visitor, object.flags);
        visit_array(visitor, object.pBindings, object.bindingCount);
    }

    void visit(
        auto&& visitor, auto&& object, tag_t<VkPushConstantRange>
    ) {
        visit(visitor, object.stageFlags);
        visit(visitor, object.offset);
        visit(visitor, object.size);
    }

    void visit(
        auto&& visitor, auto&& object, tag_t<VkPipelineLayoutCreateInfo>
    ) {
        visit(visitor, object.sType);
        //visit(visitor, object.pNext);
        visit(visitor, object.flags);
        //visit_array(visitor, object.pSetLayouts, object.setLayoutCount);
        visit_array(
            visitor, object.pPushConstantRanges, object.pushConstantRangeCount
        );
    }
}
