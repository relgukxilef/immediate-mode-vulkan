#include <immediate_mode_vulkan/resources/vulkan_resources.h>

namespace imv {
    VkInstance current_instance;
    VkDevice current_device;

    void vulkan_fence_deleter::operator()(VkFence fence) {
        vkWaitForFences(current_device, 1, &fence, VK_TRUE, ~0ul);
        // fence needs to be cleaned up regardless of whether waiting 
        // succeeded
        vkDestroyFence(current_device, fence, nullptr);
    }
}
