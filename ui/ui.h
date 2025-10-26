#pragma once

#include "resources/vulkan_resources.h"
#include "resources/vulkan_memory_allocator_resource.h"
#include <memory>
#include <vector>

struct image;
struct view;
struct ui;

struct image {
    image() = default;

    std::vector<unique_pipeline> pipelines;
    std::vector<unique_sampler> samplers;

    unique_framebuffer swapchain_framebuffer;
    unique_image_view swapchain_image_view;

    unique_semaphore render_finished_semaphore;
    unique_fence render_finished_fence;

    // TODO: descriptor sets usually aren't resolution-dependent, could be in ui
    VkDescriptorSet descriptor_set;
    VkCommandBuffer video_draw_command_buffer;
};

struct view {
    view() = default;

    VkResult render(ui &ui);

    unsigned image_count;
    VkSurfaceCapabilitiesKHR capabilities;
    VkExtent2D extent;
    unique_swapchain swapchain;
    unique_descriptor_pool descriptor_pool; // TODO: could be in ui

    std::unique_ptr<VkImage[]> swapchain_images;
    std::unique_ptr<image[]> images;
};

struct dynamic_image {
    dynamic_image() = default;
    dynamic_image(ui& ui, unsigned size);

    unique_device_memory device_memory;
    unique_image image;
    unique_image_view image_view;
    uint8_t* buffer;
};

struct ui {
    ui() = default;
    ui(
        VkInstance instance, VkPhysicalDevice physical_device, 
        VkSurfaceKHR surface
    );

    void render();

    VkPhysicalDevice physical_device;
    VkSurfaceKHR surface;

    unique_device device;
    VkQueue graphics_queue, present_queue;
    unique_command_pool command_pool;

    unique_allocator allocator;

    dynamic_image tiles;

    unique_descriptor_set_layout descriptor_set_layout;

    unique_render_pass render_pass;

    unique_pipeline_layout video_pipeline_layout;
    
    unique_buffer uniform_buffer;
    unique_allocation uniform_allocation;

    unique_semaphore swapchain_image_ready_semaphore;

    view view;

    uint32_t graphics_queue_family = ~0u, present_queue_family = ~0u;
    VkSurfaceFormatKHR surface_format;
    VkPhysicalDeviceMemoryProperties memory_properties;

    float time = 0;
};
