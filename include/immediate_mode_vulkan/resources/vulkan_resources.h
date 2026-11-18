#pragma once

#include <memory>
#include <stdexcept>
#include <stdio.h>

#include <vulkan/vulkan.h>

namespace imv {
    inline void check(VkResult result) {
        if (result != VK_SUCCESS) {
            fprintf(stderr, "Vulkan error %i\n", result);
            throw std::runtime_error("Vulkan error");
        }
    }

    extern VkDevice current_device;
    extern VkInstance current_instance;

    template<typename T, typename Deleter, auto Null = T{}>
    struct unique_resource {
        // I cannot use unique_ptr, because T must be nullable, and some Vulkan
        // handles are unsigned long long, which aren't comparable with nullptr
        typedef T pointer;

        unique_resource() : value(Null) {}
        unique_resource(T value) : value(value) {}
        unique_resource(const unique_resource&) = delete;
        unique_resource(unique_resource&& o) : value(o.value) {
            o.value = Null;
        }
        ~unique_resource() {
            if (value != Null)
                Deleter()(value);
        }
        unique_resource& operator= (const unique_resource&) = delete;
        unique_resource& operator= (unique_resource&& o) {
            if (value != Null)
                Deleter()(value);
            value = o.value;
            o.value = Null;
            return *this;
        }
        auto operator*() {
            return *value;
        }
        T* operator->() {
            return &value;
        }
        operator bool() const {
            return value != Null;
        }

        const T& get() const {
            return value;
        };

        T& get() {
            return value;
        };

        void reset(T o = Null) {
            if (value != Null)
                Deleter()(value);
            value = o;
        };

    private:
        T value;
    };

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
        void operator()(VkFence fence);
    };

    template<typename T, auto Deleter>
    struct vulkan_handle_deleter;

    template<
        typename T, void(*Deleter)(VkDevice, T, const VkAllocationCallbacks*)
    >
    struct vulkan_handle_deleter<T, Deleter> {
        typedef T pointer;
        void operator()(T object) {
            Deleter(current_device, object, nullptr);
        }
    };

    template<
        typename T, void(*Deleter)(VkInstance, T, const VkAllocationCallbacks*)
    >
    struct vulkan_handle_deleter<T, Deleter> {
        typedef T pointer;
        void operator()(T object) {
            Deleter(current_instance, object, nullptr);
        }
    };

    struct vulkan_descriptor_set_deleter {
        typedef VkDescriptorSet pointer;
        VkDescriptorPool pool;
        void operator()(VkDescriptorSet object) {
            check(vkFreeDescriptorSets(current_device, pool, 1, &object));
        }
    };

    template<typename T, auto Deleter>
    using unique_vulkan_handle = 
        unique_resource<T, vulkan_handle_deleter<T, Deleter>>;

    using unique_device = std::unique_ptr<VkDevice, vulkan_device_deleter>;
    using unique_instance = 
        std::unique_ptr<VkInstance, vulkan_instance_deleter>;
    using unique_surface = 
        unique_vulkan_handle<VkSurfaceKHR, vkDestroySurfaceKHR>;
    using unique_framebuffer = 
        unique_vulkan_handle<VkFramebuffer, vkDestroyFramebuffer>;
    using unique_image_view = 
        unique_vulkan_handle<VkImageView, vkDestroyImageView>;
    using unique_semaphore = 
        unique_vulkan_handle<VkSemaphore, vkDestroySemaphore>;
    using unique_fence = std::unique_ptr<VkFence, vulkan_fence_deleter>;
    using unique_swapchain = 
        unique_vulkan_handle<VkSwapchainKHR, vkDestroySwapchainKHR>;
    using unique_device_memory = 
        unique_vulkan_handle<VkDeviceMemory, vkFreeMemory>;
    using unique_image = unique_vulkan_handle<VkImage, vkDestroyImage>;
    using unique_image_view = 
        unique_vulkan_handle<VkImageView, vkDestroyImageView>;
    using unique_command_pool = 
        unique_vulkan_handle<VkCommandPool, vkDestroyCommandPool>;
    using unique_sampler = unique_vulkan_handle<VkSampler, vkDestroySampler>;
    using unique_descriptor_set_layout = unique_vulkan_handle<
        VkDescriptorSetLayout, vkDestroyDescriptorSetLayout
    >;
    using unique_descriptor_pool = 
        unique_vulkan_handle<VkDescriptorPool, vkDestroyDescriptorPool>;
    using unique_render_pass = 
        unique_vulkan_handle<VkRenderPass, vkDestroyRenderPass>;
    using unique_pipeline_layout = 
        unique_vulkan_handle<VkPipelineLayout, vkDestroyPipelineLayout>;
    using unique_pipeline = unique_vulkan_handle<VkPipeline, vkDestroyPipeline>;
    using unique_shader_module = 
        unique_vulkan_handle<VkShaderModule, vkDestroyShaderModule>;
    using unique_buffer = unique_vulkan_handle<VkBuffer, vkDestroyBuffer>;
    using unique_descriptor_set = 
        std::unique_ptr<VkDescriptorSet, vulkan_descriptor_set_deleter>;
    using unique_pipeline_cache = 
        unique_vulkan_handle<VkPipelineCache, vkDestroyPipelineCache>;
}
