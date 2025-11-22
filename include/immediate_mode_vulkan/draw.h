#include <vulkan/vulkan.h>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string_view>

namespace imv {
    struct renderer {
        renderer(
            VkInstance instance, VkSurfaceKHR surface
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

    struct buffer_data {
        buffer_data() = default;
        buffer_data(const void* pointer, size_t size) : 
            pointer(pointer), size(size) {}
        template<class T>
        buffer_data(const T& t) : buffer_data(&t, sizeof(t)) {}

        const void* pointer;
        size_t size;
    };

    struct vertex_binding_info {
        const void* buffer_source_pointer;
        size_t buffer_source_size;
        VkVertexInputBindingDescription description;
        std::initializer_list<VkVertexInputAttributeDescription> attributes;
    };

    struct image_info {
        std::string_view file_name;
        VkSamplerCreateInfo sampler_info;
    };

    struct draw_info {
        renderer* renderer = nullptr;
        bool prepare_only = false;
        std::initializer_list<stage_info> stages;
        std::initializer_list<vertex_binding_info> vertex_input_bindings;
        std::initializer_list<image_info> images;
        const void* uniform_source_pointer;
        VkDeviceSize uniform_source_size;
        buffer_data uniform_source;
        uint32_t vertex_count = 0;
    };

    bool draw(const draw_info&);

    void submit(renderer* renderer = nullptr);
}