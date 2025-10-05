#include "ui.h"

#include <vector>
#include <memory>
#include <algorithm>

using std::out_ptr;
using std::fill;

struct file_deleter {
    void operator()(FILE*f) const;
};

void file_deleter::operator()(FILE*f) const { fclose(f); }

std::vector<uint8_t> read_file(const char* name) {
    std::unique_ptr<FILE, file_deleter> file(fopen(name, "rb"));
    if (!file.get())
        throw std::exception();
    fseek(file.get(), 0, SEEK_END);
    auto size = ftell(file.get());
    std::vector<uint8_t> content(size);
    fseek(file.get(), 0, SEEK_SET);
    fread(content.data(), sizeof(uint8_t), size, file.get());
    return content;
}

dynamic_image::dynamic_image(ui &ui, unsigned size) {
    {
        unsigned width = size, height = size;
        VkImageCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R8G8B8A8_SRGB,
            .extent = {
                .width = width,
                .height = height,
                .depth = 1,
            },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_LINEAR,
            .usage =
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        check(vkCreateImage(
            ui.device.get(), &create_info, nullptr, out_ptr(image)
        ));
        VkMemoryRequirements memory_requirements;
        vkGetImageMemoryRequirements(
            ui.device.get(), image.get(), &memory_requirements
        );

        uint32_t memory_type = 0;
        VkMemoryPropertyFlags properties =
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        for (uint32_t i = 0; i < ui.memory_properties.memoryTypeCount; i++) {
            if (
                (memory_requirements.memoryTypeBits & (1 << i)) &&
                (
                    ui.memory_properties.memoryTypes[i].propertyFlags &
                    properties
                ) == properties
            ) {
                memory_type = i;
                break;
            }
        }

        VkMemoryAllocateInfo allocate_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memory_requirements.size,
            .memoryTypeIndex = memory_type,
        };
        check(vkAllocateMemory(
            ui.device.get(), &allocate_info, nullptr,
            out_ptr(device_memory)
        ));

        check(vkBindImageMemory(
            ui.device.get(), image.get(), device_memory.get(), 0
        ));

        check(vkMapMemory(
            ui.device.get(), device_memory.get(), 0, width * height * 4, 0,
            reinterpret_cast<void**>(&buffer)
        ));
    }

    {
        // transition image from undefined to general layout
        VkCommandBuffer command_buffer;

        VkCommandBufferAllocateInfo allocate_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = ui.command_pool.get(),
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        check(vkAllocateCommandBuffers(
            ui.device.get(), &allocate_info, &command_buffer
        ));

        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        check(vkBeginCommandBuffer(command_buffer, &begin_info));

        VkImageMemoryBarrier image_memory_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image.get(),
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };

        vkCmdPipelineBarrier(
            command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
            &image_memory_barrier
        );

        check(vkEndCommandBuffer(command_buffer));

        VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &command_buffer,
        };
        check(vkQueueSubmit(
            ui.graphics_queue, 1, &submit_info, VK_NULL_HANDLE
        ));
        check(vkQueueWaitIdle(ui.graphics_queue)); // TODO: use fence instead
        // TODO: use destructor instead
        vkFreeCommandBuffers(
            ui.device.get(), ui.command_pool.get(), 1, &command_buffer
        );
    }

    {
        VkImageViewCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image.get(),
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R8G8B8A8_SRGB,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        check(vkCreateImageView(
            ui.device.get(), &create_info, nullptr, out_ptr(image_view)
        ));
    }

    fill(buffer, buffer + size * size * 4, 255);
}

void create_shader(
    unique_device& device, const char* name,
    unique_shader_module& module
) {
    auto code = read_file(name);
    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = (size_t)(code.size()),
        .pCode = reinterpret_cast<uint32_t*>(code.data()),
    };

    check(vkCreateShaderModule(
        device.get(), &create_info, nullptr, out_ptr(module)
    ));
}

VkResult view::render(ui& ui) {
    if (!swapchain) {
        check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            ui.physical_device, ui.surface, &capabilities
        ));

        unsigned width = capabilities.currentExtent.width;
        unsigned height = capabilities.currentExtent.height;

        extent = {
            std::max(
                std::min<uint32_t>(width, capabilities.maxImageExtent.width),
                capabilities.minImageExtent.width
            ),
            std::max(
                std::min<uint32_t>(height, capabilities.maxImageExtent.height),
                capabilities.minImageExtent.height
            )
        };

        {
            uint32_t queue_family_indices[]{
                ui.graphics_queue_family, ui.present_queue_family
            };
            VkSwapchainCreateInfoKHR create_info{
                .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                .surface = ui.surface,
                .minImageCount = capabilities.minImageCount,
                .imageFormat = ui.surface_format.format,
                .imageColorSpace = ui.surface_format.colorSpace,
                .imageExtent = extent,
                .imageArrayLayers = 1,
                .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                .imageSharingMode = VK_SHARING_MODE_CONCURRENT,
                .queueFamilyIndexCount = std::size(queue_family_indices),
                .pQueueFamilyIndices = queue_family_indices,
                .preTransform = capabilities.currentTransform,
                .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                // fifo has the widest support
                .presentMode = VK_PRESENT_MODE_FIFO_KHR,
                .clipped = VK_TRUE,
                .oldSwapchain = VK_NULL_HANDLE,
            };
            check(vkCreateSwapchainKHR(
                ui.device.get(), &create_info, nullptr, out_ptr(swapchain)
            ));
        }

        check(vkGetSwapchainImagesKHR(
            ui.device.get(), swapchain.get(), &image_count, nullptr
        ));

        swapchain_images = std::make_unique<VkImage[]>(image_count);
        images = std::make_unique<image[]>(image_count);

        check(vkGetSwapchainImagesKHR(
            ui.device.get(), swapchain.get(), &image_count, 
            swapchain_images.get()
        ));
    }

    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(
        ui.device.get(), swapchain.get(), ~0ul,
        ui.swapchain_image_ready_semaphore.get(),
        VK_NULL_HANDLE, &image_index
    );
    if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR) {
        return result;
    }
    check(result);

    ::image& image = images[image_index];
    VkImage swapchain_image = swapchain_images[image_index];

    if (!image.swapchain_image_view) {
        {
            VkSemaphoreCreateInfo create_info = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            };
            check(vkCreateSemaphore(
                ui.device.get(), &create_info, nullptr,
                out_ptr(image.render_finished_semaphore)
            ));
        }

        {
            VkFenceCreateInfo create_info = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT,
            };
            check(vkCreateFence(
                ui.device.get(), &create_info, nullptr, 
                out_ptr(image.render_finished_fence)
            ));
        }

        {
            VkImageViewCreateInfo create_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = swapchain_image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = ui.surface_format.format,
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            };
            check(vkCreateImageView(
                ui.device.get(), &create_info, nullptr,
                out_ptr(image.swapchain_image_view)
            ));
        }

        {
            auto attachments = {
                image.swapchain_image_view.get(),
            };
            VkFramebufferCreateInfo create_info = {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = ui.render_pass.get(),
                .attachmentCount = static_cast<uint32_t>(attachments.size()),
                .pAttachments = attachments.begin(),
                .width = extent.width,
                .height = extent.height,
                .layers = 1,
            };
            check(vkCreateFramebuffer(
                ui.device.get(), &create_info, nullptr,
                out_ptr(image.swapchain_framebuffer)
            ));
        }

        VkCommandBufferAllocateInfo command_buffer_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = ui.command_pool.get(),
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        check(vkAllocateCommandBuffers(
            ui.device.get(), &command_buffer_info, 
            &image.video_draw_command_buffer
        ));

        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };
        check(vkBeginCommandBuffer(
            image.video_draw_command_buffer, &begin_info
        ));

        auto clear_values = {
            VkClearValue{
                .color = {{0.0f, 0.0f, 0.0f, 1.0f}},
            },
        };

        VkRenderPassBeginInfo render_pass_begin_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = ui.render_pass.get(),
            .framebuffer = image.swapchain_framebuffer.get(),
            .renderArea = {
                .offset = {0, 0}, .extent = extent,
            },
            .clearValueCount = static_cast<uint32_t>(clear_values.size()),
            .pClearValues = clear_values.begin(),
        };

        vkCmdBeginRenderPass(
            image.video_draw_command_buffer, &render_pass_begin_info,
            VK_SUBPASS_CONTENTS_INLINE
        );

        vkCmdBindPipeline(
            image.video_draw_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            ui.video_pipeline.get()
        );

        vkCmdBindDescriptorSets(
            image.video_draw_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            ui.video_pipeline_layout.get(), 0, 1, &ui.descriptor_set, 0, nullptr
        );

        vkCmdDraw(image.video_draw_command_buffer, 6, 1, 0, 0);

        vkCmdEndRenderPass(image.video_draw_command_buffer);

        check(vkEndCommandBuffer(image.video_draw_command_buffer));
    }

    auto fence = image.render_finished_fence.get();

    check(vkWaitForFences(
        ui.device.get(), 1, &fence,
        VK_TRUE, ~0ul
    ));
    check(vkResetFences(
        ui.device.get(), 1, &fence
    ));

    // TODO: maybe move this to image::render
    auto wait_semaphore = ui.swapchain_image_ready_semaphore.get();
    auto signal_semaphore = image.render_finished_semaphore.get();
    VkPipelineStageFlags wait_stage =
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &wait_semaphore,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &image.video_draw_command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &signal_semaphore,
    };
    check(vkQueueSubmit(
        ui.graphics_queue, 1, &submitInfo,
        image.render_finished_fence.get()
    ));

    auto swapchains = swapchain.get();
    VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &signal_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &swapchains,
        .pImageIndices = &image_index,
    };
    result = vkQueuePresentKHR(ui.present_queue, &present_info);
    if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR) {
        return result;
    }
    check(result);

    return VK_SUCCESS;
}

ui::ui(
    VkPhysicalDevice physical_device, VkSurfaceKHR surface
) : physical_device(physical_device), surface(surface) {
    // look for available queue families
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &queue_family_count, nullptr
    );
    auto queue_families =
        std::make_unique<VkQueueFamilyProperties[]>(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &queue_family_count, queue_families.get()
    );

    for (auto i = 0u; i < queue_family_count; i++) {
        const auto& queueFamily = queue_families[i];
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_queue_family = i;
        }

        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(
            physical_device, i, surface, &present_support
        );
        if (present_support) {
            present_queue_family = i;
        }
    }
    if (graphics_queue_family == ~0u) {
        throw std::runtime_error("no suitable queue found");
    }


    // create logical device
    {
        float priority = 1.0f;
        VkDeviceQueueCreateInfo queue_create_infos[]{
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = graphics_queue_family,
                .queueCount = 1,
                .pQueuePriorities = &priority,
            }, {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = present_queue_family,
                .queueCount = 1,
                .pQueuePriorities = &priority,
            }
        };

        const char* enabled_extension_names[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };

        VkPhysicalDeviceFeatures device_features{};
        VkDeviceCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = std::size(queue_create_infos),
            .pQueueCreateInfos = queue_create_infos,
            .enabledExtensionCount = std::size(enabled_extension_names),
            .ppEnabledExtensionNames = enabled_extension_names,
            .pEnabledFeatures = &device_features
        };

        check(vkCreateDevice(
            physical_device, &create_info, nullptr, out_ptr(device)
        ));
    }
    current_device = device.get();

    // retreive queues
    vkGetDeviceQueue(device.get(), graphics_queue_family, 0, &graphics_queue);
    vkGetDeviceQueue(device.get(), present_queue_family, 0, &present_queue);

    // create swap chains
    uint32_t format_count = 0, present_mode_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        physical_device, surface, &format_count, nullptr
    );
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        physical_device, surface, &present_mode_count, nullptr
    );
    if (format_count == 0) {
        throw std::runtime_error("no surface formats supported");
    }
    if (present_mode_count == 0) {
        throw std::runtime_error("no surface present modes supported");
    }
    auto formats = std::make_unique<VkSurfaceFormatKHR[]>(format_count);
    auto present_modes =
        std::make_unique<VkPresentModeKHR[]>(present_mode_count);

    vkGetPhysicalDeviceSurfaceFormatsKHR(
        physical_device, surface, &format_count, formats.get()
    );
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        physical_device, surface, &present_mode_count, present_modes.get()
    );

    surface_format = formats[0];
    for (auto i = 0u; i < format_count; i++) {
        auto format = formats[i];
        if (
            format.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        ) {
            surface_format = format;
        }
    }

    {
        VkCommandPoolCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = graphics_queue_family,
        };
        check(vkCreateCommandPool(
            device.get(), &create_info, nullptr, out_ptr(command_pool)
        ));
    }

    unique_shader_module video_vertex, video_fragment;
    create_shader(
        device, "ui/video_vertex.glsl.spv", video_vertex
    );
    create_shader(
        device, "ui/video_fragment.glsl.spv", video_fragment
    );

    vkGetPhysicalDeviceMemoryProperties(
        physical_device, &memory_properties
    );

    tiles = dynamic_image(*this, 2048);

    {
        auto descriptor_set_layout_binding = {
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            }, {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            }, {
                .binding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
        };
        VkDescriptorSetLayoutCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount =
                static_cast<uint32_t>(descriptor_set_layout_binding.size()),
            .pBindings = descriptor_set_layout_binding.begin(),
        };
        check(vkCreateDescriptorSetLayout(
            device.get(), &create_info,
            nullptr, out_ptr(descriptor_set_layout)
        ));
    }

    {
        VkDescriptorPoolSize pool_size = {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
        };
        VkDescriptorPoolCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &pool_size,
        };
        check(vkCreateDescriptorPool(
            device.get(), &create_info, nullptr, out_ptr(descriptor_pool))
        );
    }

    {
        auto layout = descriptor_set_layout.get();
        VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = descriptor_pool.get(),
            .descriptorSetCount = 1,
            .pSetLayouts = &layout,
        };
        check(vkAllocateDescriptorSets(
            device.get(), &descriptor_set_allocate_info, &descriptor_set
        ));
    }

    {
        VkSamplerCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .anisotropyEnable = VK_FALSE,
        };
        check(vkCreateSampler(
            device.get(), &create_info, nullptr, out_ptr(tiles_sampler)
        ));
    }

    {
        auto descriptor_buffer_info = {
            VkDescriptorImageInfo{
                .sampler = tiles_sampler.get(),
                .imageView = tiles.image_view.get(),
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            },
        };
        VkWriteDescriptorSet write_descriptor_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount =
                static_cast<uint32_t>(descriptor_buffer_info.size()),
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = descriptor_buffer_info.begin(),
        };
        vkUpdateDescriptorSets(
            device.get(), 1, &write_descriptor_set, 0, nullptr
        );
    }

    {
        auto layout = descriptor_set_layout.get();
        VkPipelineLayoutCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &layout,
        };
        check(vkCreatePipelineLayout(
            device.get(), &create_info, nullptr, out_ptr(video_pipeline_layout)
        ));
    }

    {
        auto attachments = {
            VkAttachmentDescription{
                .format = surface_format.format,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
        };
        auto attachment_references = {
            VkAttachmentReference{
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
        };
        auto subpasses = {
            VkSubpassDescription{
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount =
                    static_cast<uint32_t>(attachment_references.size()),
                .pColorAttachments = attachment_references.begin(),
            },
        };
        auto subpass_dependencies = {
            VkSubpassDependency{
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            },
        };
        VkRenderPassCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments = attachments.begin(),
            .subpassCount = static_cast<uint32_t>(subpasses.size()),
            .pSubpasses = subpasses.begin(),
            .dependencyCount =
                static_cast<uint32_t>(subpass_dependencies.size()),
            .pDependencies = subpass_dependencies.begin(),
        };
        check(vkCreateRenderPass(
            device.get(), &create_info, nullptr, out_ptr(render_pass)
        ));
    }

    {
        auto shader_stages = {
            VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = video_vertex.get(),
                .pName = "main",
            }, VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = video_fragment.get(),
                .pName = "main",
            },
        };
        VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        };
        VkPipelineInputAssemblyStateCreateInfo pipeline_input_assembly_state = {
            .sType =
                VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
            .primitiveRestartEnable = VK_FALSE,
        };
        VkViewport viewport = {
            .x = 0.0f, .y = 0.0f,
            .width = 1280.0f, .height = 720.0f,
            .minDepth = 0.0f, .maxDepth = 1.0f,
        };
        VkRect2D scissor = {.offset = {0, 0}, .extent = {1280, 720},};
        VkPipelineViewportStateCreateInfo pipeline_viewport_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .pViewports = &viewport,
            .scissorCount = 1,
            .pScissors = &scissor,
        };
        VkPipelineRasterizationStateCreateInfo pipeline_rasterization_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .lineWidth = 1.0f, // required, even when not doing line rendering
        };
        VkPipelineMultisampleStateCreateInfo pipeline_multisample_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        };
        auto pipeline_color_blend_attachment_states = {
            VkPipelineColorBlendAttachmentState{
                .colorWriteMask =
                    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            },
        };
        VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = static_cast<uint32_t>(
                pipeline_color_blend_attachment_states.size()
            ),
            .pAttachments = pipeline_color_blend_attachment_states.begin(),
            .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
        };
        VkGraphicsPipelineCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = static_cast<uint32_t>(shader_stages.size()),
            .pStages = shader_stages.begin(),
            .pVertexInputState = &pipeline_vertex_input_state,
            .pInputAssemblyState = &pipeline_input_assembly_state,
            .pViewportState = &pipeline_viewport_state,
            .pRasterizationState = &pipeline_rasterization_state,
            .pMultisampleState = &pipeline_multisample_state,
            .pColorBlendState = &pipeline_color_blend_state,
            .layout = video_pipeline_layout.get(),
            .renderPass = render_pass.get(),
        };
        check(vkCreateGraphicsPipelines(
            device.get(), nullptr, 1, &create_info, nullptr,
            out_ptr(video_pipeline)
        ));
    }

    {
        VkSemaphoreCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };
        check(vkCreateSemaphore(
            device.get(), &create_info, nullptr,
            out_ptr(swapchain_image_ready_semaphore)
        ));
    }
}

void ui::render() {
    VkResult result = view.render(*this);
    if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR) {
        view = {}; // delete first

        view.render(*this);
    }
}
