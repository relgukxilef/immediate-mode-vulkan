#include <vulkan/vulkan.h>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string_view>

namespace imv {
    struct renderer {
        renderer(
            VkInstance instance, VkPhysicalDevice physical_device, 
            VkSurfaceKHR surface
        );
        ~renderer();

        std::unique_ptr<struct renderer_data> d;
    };

    extern struct renderer* global_renderer;

    void wait_frame(renderer* renderer = nullptr);

    struct stage_info {
        // TODO: replace with string_view
        const char* code_file_name;
        VkPipelineShaderStageCreateInfo info;
    };

    struct image_info {
        std::string_view file_name;
        VkSamplerCreateInfo sampler_info;
    };

    struct draw_info {
        // TODO: maybe use snake case everywhere
        renderer* renderer = nullptr;
        bool prepare_only = false;
        std::initializer_list<stage_info> stages;
        std::initializer_list<image_info> images;
        const void* uniform_source_pointer;
        VkDeviceSize uniform_source_size;
        uint32_t vertex_count = 0;
    };

    bool draw(const draw_info&);

    void submit(renderer* renderer = nullptr);
}