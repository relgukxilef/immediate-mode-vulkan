#pragma once

#include <vk_mem_alloc.h>

#include <memory>

namespace imv {
    extern VmaAllocator current_allocator;

    struct vulkan_memory_allocator_deleter {
        typedef VmaAllocator pointer;
        void operator()(VmaAllocator allocator) {
            vmaDestroyAllocator(allocator);
        }
    };


    template<typename T, void(*Deleter)(VmaAllocator, T)>
    struct vulkan_memory_allocator_handle_deleter {
        typedef T pointer;
        void operator()(T object) {
            Deleter(current_allocator, object);
        }
    };

    template<typename T, auto Deleter>
    using unique_vulkan_memory_allocator_handle = 
        std::unique_ptr<T, vulkan_memory_allocator_handle_deleter<T, Deleter>>;

    using unique_allocator = 
        std::unique_ptr<VmaAllocator, vulkan_memory_allocator_deleter>;

    using unique_allocation = 
        unique_vulkan_memory_allocator_handle<VmaAllocation, vmaFreeMemory>;
}