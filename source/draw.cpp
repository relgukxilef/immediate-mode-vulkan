#include <immediate_mode_vulkan/draw.h>
#include <immediate_mode_vulkan/resources/vulkan_resources.h>
#include <immediate_mode_vulkan/resources/vulkan_memory_allocator_resource.h>

#include <vector>

using namespace std;

namespace imv {

    renderer* global_renderer;

    size_t aligned(size_t size, size_t alignment) {
        return alignment * ((size - 1) / alignment + 1);
    }

    struct image {
        std::vector<unique_pipeline> pipelines;
        std::vector<unique_sampler> samplers;

        // TODO: allocate uniform data from a shared buffer
        size_t uniform_buffer_size = 0, uniform_buffer_capacity = 1024 * 1024;
        unique_buffer uniform_buffer;
        unique_allocation uniform_allocation;

        unique_framebuffer swapchain_framebuffer;
        unique_image_view swapchain_image_view;

        unique_semaphore render_finished_semaphore;
        unique_fence render_finished_fence;

        std::vector<VkDescriptorSet> descriptor_sets;
        VkCommandBuffer command_buffer;
    };

    struct view {
        unsigned image_count;
        VkSurfaceCapabilitiesKHR capabilities;
        VkExtent2D extent;
        unique_swapchain swapchain;
        unique_descriptor_pool descriptor_pool; // TODO: could be in r

        std::unique_ptr<VkImage[]> swapchain_images;
        std::unique_ptr<image[]> images;
        uint32_t image_index;
    };

    struct renderer_data;

    struct dynamic_image {
        dynamic_image() = default;
        dynamic_image(renderer_data& renderer, unsigned size);

        unique_device_memory device_memory;
        unique_image image;
        unique_image_view image_view;
        uint8_t* buffer;
    };

    struct renderer_data {
        VkPhysicalDevice physical_device;
        VkSurfaceKHR surface;

        size_t offset_alignment;

        unique_device device;
        VkQueue graphics_queue, present_queue;
        unique_command_pool command_pool;

        unique_allocator allocator;

        dynamic_image tiles;

        unique_descriptor_set_layout descriptor_set_layout;

        unique_render_pass render_pass;

        unique_pipeline_layout video_pipeline_layout;
        
        unique_semaphore swapchain_image_ready_semaphore;

        view view;

        uint32_t graphics_queue_family = ~0u, present_queue_family = ~0u;
        VkSurfaceFormatKHR surface_format;
        VkPhysicalDeviceMemoryProperties memory_properties;
        
        vector<unique_shader_module> shader_modules;
        vector<VkPipelineShaderStageCreateInfo> pipeline_shader_stages;
    };

    struct file_deleter {
        void operator()(FILE* f) const;
    };

    void file_deleter::operator()(FILE* f) const { fclose(f); }

    std::vector<uint8_t> read_file(const char* name) {
        std::unique_ptr<FILE, file_deleter> file(fopen(name, "rb"));
        if (!file.get())
            throw std::exception();
        fseek(file.get(), 0, SEEK_END);
        auto size = size_t(ftell(file.get()));
        std::vector<uint8_t> content(size);
        fseek(file.get(), 0, SEEK_SET);
        fread(content.data(), sizeof(uint8_t), size, file.get());
        return content;
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

    dynamic_image::dynamic_image(renderer_data& r, unsigned size) {
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
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | 
                    VK_IMAGE_USAGE_SAMPLED_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            };
            check(vkCreateImage(
                r.device.get(), &create_info, nullptr, out_ptr(image)
            ));
            VkMemoryRequirements memory_requirements;
            vkGetImageMemoryRequirements(
                r.device.get(), image.get(), &memory_requirements
            );

            uint32_t memory_type = 0;
            VkMemoryPropertyFlags properties =
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

            for (uint32_t i = 0; i < r.memory_properties.memoryTypeCount; i++) {
                if (
                    (memory_requirements.memoryTypeBits & (1 << i)) &&
                    (
                        r.memory_properties.memoryTypes[i].propertyFlags &
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
                r.device.get(), &allocate_info, nullptr,
                out_ptr(device_memory)
            ));

            check(vkBindImageMemory(
                r.device.get(), image.get(), device_memory.get(), 0
            ));

            check(vkMapMemory(
                r.device.get(), device_memory.get(), 0, width * height * 4, 0,
                reinterpret_cast<void**>(&buffer)
            ));
        }

        {
            // transition image from undefined to general layout
            VkCommandBuffer command_buffer;

            VkCommandBufferAllocateInfo allocate_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = r.command_pool.get(),
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            };
            check(vkAllocateCommandBuffers(
                r.device.get(), &allocate_info, &command_buffer
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
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, 
                nullptr, 1,
                &image_memory_barrier
            );

            check(vkEndCommandBuffer(command_buffer));

            VkSubmitInfo submit_info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &command_buffer,
            };
            check(vkQueueSubmit(
                r.graphics_queue, 1, &submit_info, VK_NULL_HANDLE
            ));
            check(vkQueueWaitIdle(r.graphics_queue)); // TODO: use fence instead
            // TODO: use destructor instead
            vkFreeCommandBuffers(
                r.device.get(), r.command_pool.get(), 1, &command_buffer
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
                r.device.get(), &create_info, nullptr, out_ptr(image_view)
            ));
        }

        fill(buffer, buffer + size * size * 4, 255);
    }

    renderer::renderer(
        VkInstance instance, VkPhysicalDevice physical_device, 
        VkSurfaceKHR surface
    ) {
        d = make_unique<renderer_data>();
        d->physical_device = physical_device;
        d->surface = surface;
        auto &r = *d;

        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physical_device, &properties);
        d->offset_alignment = properties.limits.minUniformBufferOffsetAlignment;

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
                r.graphics_queue_family = i;
            }

            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(
                physical_device, i, surface, &present_support
            );
            if (present_support) {
                r.present_queue_family = i;
            }
        }
        if (r.graphics_queue_family == ~0u) {
            throw std::runtime_error("no suitable queue found");
        }


        // create logical device
        {
            float priority = 1.0f;
            VkDeviceQueueCreateInfo queue_create_infos[]{
                {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = r.graphics_queue_family,
                    .queueCount = 1,
                    .pQueuePriorities = &priority,
                }, {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = r.present_queue_family,
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
                r.physical_device, &create_info, nullptr, out_ptr(r.device)
            ));
        }
        current_device = r.device.get();

        // retrieve queues
        vkGetDeviceQueue(
            r.device.get(), r.graphics_queue_family, 0, &r.graphics_queue
        );
        vkGetDeviceQueue(
            r.device.get(), r.present_queue_family, 0, &r.present_queue
        );

        // create allocator
        {
            VmaVulkanFunctions vulkan_functions = {
                .vkGetInstanceProcAddr = &vkGetInstanceProcAddr,
                .vkGetDeviceProcAddr = &vkGetDeviceProcAddr,
            };

            VmaAllocatorCreateInfo create_info = {
                .physicalDevice = physical_device,
                .device = r.device.get(),
                .pVulkanFunctions = &vulkan_functions,
                .instance = instance,
                .vulkanApiVersion = VK_API_VERSION_1_0,
            };
            check(vmaCreateAllocator(&create_info, out_ptr(r.allocator)));
            current_allocator = r.allocator.get();
        }

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

        r.surface_format = formats[0];
        for (auto i = 0u; i < format_count; i++) {
            auto format = formats[i];
            if (
                format.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 &&
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            ) {
                r.surface_format = format;
            }
        }

        {
            VkCommandPoolCreateInfo create_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = r.graphics_queue_family,
            };
            check(vkCreateCommandPool(
                r.device.get(), &create_info, nullptr, out_ptr(r.command_pool)
            ));
        }

        vkGetPhysicalDeviceMemoryProperties(
            r.physical_device, &r.memory_properties
        );

        r.tiles = dynamic_image(r, 2048);

        {
            auto attachments = {
                VkAttachmentDescription{
                    .format = r.surface_format.format,
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
                    .srcStageMask = 
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .dstStageMask = 
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
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
                r.device.get(), &create_info, nullptr, out_ptr(r.render_pass)
            ));
        }

        {
            VkSemaphoreCreateInfo create_info = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            };
            check(vkCreateSemaphore(
                r.device.get(), &create_info, nullptr,
                out_ptr(r.swapchain_image_ready_semaphore)
            ));
        }
    }

    renderer::~renderer() {
    }

    renderer& get(renderer* renderer) {
        if (!renderer)
            renderer = global_renderer;
        return *renderer;
    }

    void wait_frame(renderer* renderer) {
        renderer_data& r = *get(renderer).d;
        auto& view = r.view;

        if (!view.swapchain) {
            check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                r.physical_device, r.surface, &view.capabilities
            ));

            unsigned width = view.capabilities.currentExtent.width;
            unsigned height = view.capabilities.currentExtent.height;

            view.extent = {
                std::max(
                    std::min<uint32_t>(
                        width, view.capabilities.maxImageExtent.width
                    ),
                    view.capabilities.minImageExtent.width
                ),
                std::max(
                    std::min<uint32_t>(
                        height, view.capabilities.maxImageExtent.height
                    ),
                    view.capabilities.minImageExtent.height
                )
            };

            {
                uint32_t queue_family_indices[]{
                    r.graphics_queue_family, r.present_queue_family
                };
                VkSwapchainCreateInfoKHR create_info{
                    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                    .surface = r.surface,
                    .minImageCount = max(
                        min(3u, view.capabilities.maxImageCount), 
                        view.capabilities.minImageCount
                    ),
                    .imageFormat = r.surface_format.format,
                    .imageColorSpace = r.surface_format.colorSpace,
                    .imageExtent = view.extent,
                    .imageArrayLayers = 1,
                    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                    .imageSharingMode = VK_SHARING_MODE_CONCURRENT,
                    .queueFamilyIndexCount = std::size(queue_family_indices),
                    .pQueueFamilyIndices = queue_family_indices,
                    .preTransform = view.capabilities.currentTransform,
                    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                    // fifo has the widest support
                    .presentMode = VK_PRESENT_MODE_FIFO_KHR,
                    .clipped = VK_TRUE,
                    .oldSwapchain = VK_NULL_HANDLE,
                };
                check(vkCreateSwapchainKHR(
                    r.device.get(), &create_info, nullptr, 
                    out_ptr(view.swapchain)
                ));
            }

            check(vkGetSwapchainImagesKHR(
                r.device.get(), view.swapchain.get(), &view.image_count, nullptr
            ));

            view.swapchain_images = make_unique<VkImage[]>(view.image_count);
            view.images = make_unique<image[]>(view.image_count);

            check(vkGetSwapchainImagesKHR(
                r.device.get(), view.swapchain.get(), &view.image_count, 
                view.swapchain_images.get()
            ));
        }

        VkResult result = vkAcquireNextImageKHR(
            r.device.get(), view.swapchain.get(), ~0ul,
            r.swapchain_image_ready_semaphore.get(),
            VK_NULL_HANDLE, &view.image_index
        );
        if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR) {
            std::exchange(view, {});
            return;
        }
        check(result);

        imv::image& image = view.images[view.image_index];
        VkImage swapchain_image = view.swapchain_images[view.image_index];

        if (!image.swapchain_image_view) {
            {
                VkSemaphoreCreateInfo create_info = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                };
                check(vkCreateSemaphore(
                    r.device.get(), &create_info, nullptr,
                    out_ptr(image.render_finished_semaphore)
                ));
            }

            {
                VkFenceCreateInfo create_info = {
                    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                    .flags = VK_FENCE_CREATE_SIGNALED_BIT,
                };
                check(vkCreateFence(
                    r.device.get(), &create_info, nullptr, 
                    out_ptr(image.render_finished_fence)
                ));
            }

            {
                VkImageViewCreateInfo create_info = {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image = swapchain_image,
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = r.surface_format.format,
                    .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                };
                check(vkCreateImageView(
                    r.device.get(), &create_info, nullptr,
                    out_ptr(image.swapchain_image_view)
                ));
            }

            {
                auto attachments = {
                    image.swapchain_image_view.get(),
                };
                VkFramebufferCreateInfo create_info = {
                    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                    .renderPass = r.render_pass.get(),
                    .attachmentCount = uint32_t(attachments.size()),
                    .pAttachments = attachments.begin(),
                    .width = view.extent.width,
                    .height = view.extent.height,
                    .layers = 1,
                };
                check(vkCreateFramebuffer(
                    r.device.get(), &create_info, nullptr,
                    out_ptr(image.swapchain_framebuffer)
                ));
            }

            VkCommandBufferAllocateInfo command_buffer_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = r.command_pool.get(),
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            };
            check(vkAllocateCommandBuffers(
                r.device.get(), &command_buffer_info, 
                &image.command_buffer
            ));
        }

        auto fence = image.render_finished_fence.get();

        check(vkWaitForFences(
            r.device.get(), 1, &fence,
            VK_TRUE, ~0ul
        ));
        check(vkResetFences(
            r.device.get(), 1, &fence
        ));

        bool recording = !image.command_buffer;
        recording = true;
        if (recording) {
            vkResetCommandBuffer(image.command_buffer, 0);
            image.pipelines.clear();
            image.samplers.clear();
            if (!image.descriptor_sets.empty())
                vkFreeDescriptorSets(
                    r.device.get(), view.descriptor_pool.get(), 
                    image.descriptor_sets.size(), 
                    image.descriptor_sets.data()
                );
            image.descriptor_sets.clear();
            image.uniform_buffer_size = 0;

            VkCommandBufferBeginInfo begin_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            };
            check(vkBeginCommandBuffer(
                image.command_buffer, &begin_info
            ));
            
            auto clear_values = {
                VkClearValue{
                    .color = {{0.0f, 0.0f, 0.0f, 1.0f}},
                },
            };

            VkRenderPassBeginInfo render_pass_begin_info = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass = r.render_pass.get(),
                .framebuffer = image.swapchain_framebuffer.get(),
                .renderArea = {
                    .offset = {0, 0}, .extent = view.extent,
                },
                .clearValueCount = static_cast<uint32_t>(clear_values.size()),
                .pClearValues = clear_values.begin(),
            };

            vkCmdBeginRenderPass(
                image.command_buffer, &render_pass_begin_info,
                VK_SUBPASS_CONTENTS_INLINE
            );
        }
    }

    bool draw(const draw_info& info) {
        renderer_data& r = *get(info.renderer).d;
        auto& view = r.view;
        if (!view.images)
            return false;
        bool recording = true; // TODO
        imv::image& image = view.images[view.image_index];

        VkDeviceSize uniform_size = 128;

        if (!r.video_pipeline_layout) {
            {
                auto descriptor_set_layout_binding = {
                    VkDescriptorSetLayoutBinding{
                        .binding = 0,
                        .descriptorType = 
                            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .descriptorCount = 1,
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                    }, {
                        .binding = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        .descriptorCount = 1,
                        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                    },
                };
                VkDescriptorSetLayoutCreateInfo create_info = {
                    .sType = 
                        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                    .bindingCount =
                        uint32_t(descriptor_set_layout_binding.size()),
                    .pBindings = descriptor_set_layout_binding.begin(),
                };
                check(vkCreateDescriptorSetLayout(
                    r.device.get(), &create_info,
                    nullptr, out_ptr(r.descriptor_set_layout)
                ));
            }

            {
                auto layout = r.descriptor_set_layout.get();
                VkPipelineLayoutCreateInfo create_info = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                    .setLayoutCount = 1,
                    .pSetLayouts = &layout,
                };
                check(vkCreatePipelineLayout(
                    r.device.get(), &create_info, nullptr, 
                    out_ptr(r.video_pipeline_layout)
                ));
            }
        }

        if (!image.uniform_buffer) {
            {
                VkBufferCreateInfo create_info {
                    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                    .size = image.uniform_buffer_capacity,
                    .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                };
                VmaAllocationCreateInfo allocation_create_info {
                    .flags = 
                        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                    .usage = VMA_MEMORY_USAGE_AUTO,
                };
                check(vmaCreateBuffer(
                    r.allocator.get(), &create_info, &allocation_create_info,
                    out_ptr(image.uniform_buffer), 
                    out_ptr(image.uniform_allocation),
                    nullptr
                ));
            }
        }

        if (recording) {
            image.samplers.push_back({});

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
                    .minLod = 0.0,
                    .maxLod = VK_LOD_CLAMP_NONE,
                };
                check(vkCreateSampler(
                    r.device.get(), &create_info, nullptr, 
                    out_ptr(image.samplers.back())
                ));
            }

            image.pipelines.push_back({});
            
            // TODO: use VkPipelineCache
            r.shader_modules.resize(info.stages.size());
            r.pipeline_shader_stages.resize(info.stages.size());

            for (auto i = 0u; i < info.stages.size(); i++) {
                create_shader(
                    r.device, (info.stages.begin() + i)->codeFileName, 
                    r.shader_modules[i]
                );
                VkPipelineShaderStageCreateInfo create_info = 
                    (info.stages.begin() + i)->info;
                create_info.sType = 
                    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                create_info.module = r.shader_modules[i].get();
                if (create_info.pName == nullptr)
                    create_info.pName = "main";
                r.pipeline_shader_stages[i] = create_info;
            }

            VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state = {
                .sType = 
                    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            };
            VkPipelineInputAssemblyStateCreateInfo 
            pipeline_input_assembly_state = {
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
            VkPipelineRasterizationStateCreateInfo 
            pipeline_rasterization_state = {
                .sType = 
                    VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .polygonMode = VK_POLYGON_MODE_FILL,
                // required, even when not doing line rendering
                .lineWidth = 1.0f, 
            };
            VkPipelineMultisampleStateCreateInfo pipeline_multisample_state = {
                .sType = 
                    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
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
                .sType = 
                    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .attachmentCount = static_cast<uint32_t>(
                    pipeline_color_blend_attachment_states.size()
                ),
                .pAttachments = pipeline_color_blend_attachment_states.begin(),
                .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
            };
            VkGraphicsPipelineCreateInfo create_info = {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .stageCount = uint32_t(r.pipeline_shader_stages.size()),
                .pStages = r.pipeline_shader_stages.data(),
                .pVertexInputState = &pipeline_vertex_input_state,
                .pInputAssemblyState = &pipeline_input_assembly_state,
                .pViewportState = &pipeline_viewport_state,
                .pRasterizationState = &pipeline_rasterization_state,
                .pMultisampleState = &pipeline_multisample_state,
                .pColorBlendState = &pipeline_color_blend_state,
                .layout = r.video_pipeline_layout.get(),
                .renderPass = r.render_pass.get(),
            };
            check(vkCreateGraphicsPipelines(
                r.device.get(), nullptr, 1, &create_info, nullptr,
                out_ptr(image.pipelines.back())
            ));
            r.shader_modules.clear();
        }

        if (!view.descriptor_pool) {
            // TODO: may need a separate pool per pipeline layout
            unsigned max_draw_count = 1024; // TODO: grow
            VkDescriptorPoolSize pool_size[] = {
                {
                    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1,
                }, {
                    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 1,
                }, 
            };
            VkDescriptorPoolCreateInfo create_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT ,
                .maxSets = view.image_count * max_draw_count,
                .poolSizeCount = std::size(pool_size),
                .pPoolSizes = pool_size,
            };
            check(vkCreateDescriptorPool(
                r.device.get(), &create_info, nullptr, 
                out_ptr(view.descriptor_pool))
            );
        }

        if (recording) {
            image.descriptor_sets.push_back({});
            auto layout = r.descriptor_set_layout.get();
            VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = view.descriptor_pool.get(),
                .descriptorSetCount = 1,
                .pSetLayouts = &layout,
            };
            check(vkAllocateDescriptorSets(
                r.device.get(), &descriptor_set_allocate_info, 
                &image.descriptor_sets.back()
            ));
            VkDescriptorImageInfo descriptor_image_info[] = {
                {
                    .sampler = image.samplers.back().get(),
                    .imageView = r.tiles.image_view.get(),
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                },
            };
            VkDescriptorBufferInfo descriptor_buffer_info[] = {
                {
                    .buffer = image.uniform_buffer.get(),
                    .offset = image.uniform_buffer_size,
                    .range = uniform_size,
                }
            };
            VkWriteDescriptorSet write_descriptor_set[] = {
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = image.descriptor_sets.back(),
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = uint32_t(size(descriptor_image_info)),
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = descriptor_image_info,
                }, {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = image.descriptor_sets.back(),
                    .dstBinding = 1,
                    .dstArrayElement = 0,
                    .descriptorCount = uint32_t(size(descriptor_buffer_info)),
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .pBufferInfo = descriptor_buffer_info,
                }
            };
            vkUpdateDescriptorSets(
                r.device.get(), 
                size(write_descriptor_set), write_descriptor_set, 0, nullptr
            );

            vkCmdBindPipeline(
                image.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                image.pipelines.back().get()
            );

            vkCmdBindDescriptorSets(
                image.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                r.video_pipeline_layout.get(), 0, 1, 
                &image.descriptor_sets.back(), 0, nullptr
            );

            vkCmdDraw(image.command_buffer, info.vertexCount, 1, 0, 0);
        }

        // TODO: store offset in uniform_buffer?
        check(vmaCopyMemoryToAllocation(
            r.allocator.get(), info.copyMemoryToUniformBufferSrcHostPointer, 
            image.uniform_allocation.get(), 
            image.uniform_buffer_size, info.copyMemoryToUniformBufferSize
        ));

        if (recording) {
            image.uniform_buffer_size += 
                aligned(info.copyMemoryToUniformBufferSize, r.offset_alignment);
        }

        return true;
    }

    void submit(renderer* renderer) {
        renderer_data& r = *get(renderer).d;
        auto& view = r.view;
        if (!view.images)
            return;
        imv::image& image = view.images[view.image_index];
        bool recording = true; // TODO

        if (recording) {
            vkCmdEndRenderPass(image.command_buffer);

            check(vkEndCommandBuffer(image.command_buffer));
        }

        auto wait_semaphore = r.swapchain_image_ready_semaphore.get();
        auto signal_semaphore = image.render_finished_semaphore.get();
        VkPipelineStageFlags wait_stage =
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &wait_semaphore,
            .pWaitDstStageMask = &wait_stage,
            .commandBufferCount = 1,
            .pCommandBuffers = &image.command_buffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &signal_semaphore,
        };
        check(vkQueueSubmit(
            r.graphics_queue, 1, &submitInfo,
            image.render_finished_fence.get()
        ));

        auto swapchains = view.swapchain.get();
        VkPresentInfoKHR present_info{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &signal_semaphore,
            .swapchainCount = 1,
            .pSwapchains = &swapchains,
            .pImageIndices = &view.image_index,
        };
        VkResult result = vkQueuePresentKHR(r.present_queue, &present_info);
        if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR) {
            std::exchange(view, {});
            return;
        }
        check(result);
    }
}
