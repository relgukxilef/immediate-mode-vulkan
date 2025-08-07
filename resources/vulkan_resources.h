#pragma once

#include <memory>
#include <stdexcept>

#include <vulkan/vulkan.h>

inline void check(VkResult result) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Vulkan error");
    }
}

extern VkDevice current_device;
extern VkInstance current_instance;

struct vulkan_device_deleter {
    typedef VkDevice pointer;
    void operator()(VkDevice device) {
        vkDestroyDevice(device, nullptr);
    }
};

struct vulkan_instance_deleter {
    typedef VkInstance pointer;
    void operator()(VkInstance device) {
        vkDestroyInstance(device, nullptr);
    }
};

struct vulkan_fence_deleter {
    typedef VkFence pointer;
    void operator()(VkFence fence) {
        VkResult result = 
            vkWaitForFences(current_device, 1, &fence, VK_TRUE, ~0ul);
        // fence needs to be cleaned up regardless of whether waiting succeeded
        vkDestroyFence(current_device, fence, nullptr);
        if (std::uncaught_exceptions())
            // Destructor was called during stack unwinding, throwing a new 
            // expcetion would terminate the application.
            return;
        check(result);
    }
};

template<typename T, auto Deleter>
struct vulkan_handle_deleter;

template<typename T, void(*Deleter)(VkDevice, T, const VkAllocationCallbacks*)>
struct vulkan_handle_deleter<T, Deleter> {
    typedef T pointer;
    void operator()(T object) {
        Deleter(current_device, object, nullptr);
    }
};

template<typename T, void(*Deleter)(VkInstance, T, const VkAllocationCallbacks*)>
struct vulkan_handle_deleter<T, Deleter> {
    typedef T pointer;
    void operator()(T object) {
        Deleter(current_instance, object, nullptr);
    }
};

template<typename T, auto Deleter>
using unique_vulkan_handle = 
    std::unique_ptr<T, vulkan_handle_deleter<T, Deleter>>;

using unique_device = std::unique_ptr<VkDevice, vulkan_device_deleter>;
using unique_instance = std::unique_ptr<VkInstance, vulkan_instance_deleter>;
using unique_surface = unique_vulkan_handle<VkSurfaceKHR, vkDestroySurfaceKHR>;
using unique_framebuffer = 
    unique_vulkan_handle<VkFramebuffer, vkDestroyFramebuffer>;
using unique_image_view = unique_vulkan_handle<VkImageView, vkDestroyImageView>;
using unique_semaphore = unique_vulkan_handle<VkSemaphore, vkDestroySemaphore>;
using unique_fence = std::unique_ptr<VkFence, vulkan_fence_deleter>;
using unique_swapchain = 
    unique_vulkan_handle<VkSwapchainKHR, vkDestroySwapchainKHR>;
using unique_device_memory = unique_vulkan_handle<VkDeviceMemory, vkFreeMemory>;
using unique_image = unique_vulkan_handle<VkImage, vkDestroyImage>;
using unique_image_view = unique_vulkan_handle<VkImageView, vkDestroyImageView>;
using unique_command_pool = 
    unique_vulkan_handle<VkCommandPool, vkDestroyCommandPool>;
using unique_sampler = unique_vulkan_handle<VkSampler, vkDestroySampler>;
using unique_descriptor_set_layout = 
    unique_vulkan_handle<VkDescriptorSetLayout, vkDestroyDescriptorSetLayout>;
using unique_descriptor_pool = 
    unique_vulkan_handle<VkDescriptorPool, vkDestroyDescriptorPool>;
using unique_render_pass = 
    unique_vulkan_handle<VkRenderPass, vkDestroyRenderPass>;
using unique_pipeline_layout = 
    unique_vulkan_handle<VkPipelineLayout, vkDestroyPipelineLayout>;
using unique_pipeline = unique_vulkan_handle<VkPipeline, vkDestroyPipeline>;
using unique_shader_module = 
    unique_vulkan_handle<VkShaderModule, vkDestroyShaderModule>;
