#include <immediate_mode_vulkan/resources/vulkan_memory_allocator_resource.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

VmaAllocator imv::current_allocator = VK_NULL_HANDLE;
