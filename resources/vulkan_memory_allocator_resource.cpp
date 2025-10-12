#include "vulkan_memory_allocator_resource.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

VmaAllocator current_allocator = VK_NULL_HANDLE;
