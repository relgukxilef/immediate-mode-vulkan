#include <vulkan/vulkan.h>
#include <cstdint>
#include <initializer_list>
#include <memory>

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
        const char* codeFileName;
        VkPipelineShaderStageCreateInfo info;
    };

    struct draw_info {
        renderer* renderer = nullptr;
        bool prepare_only = false;
        const VkDescriptorSetLayoutCreateInfo* descriptor_set_layout;
        std::initializer_list<stage_info> stages;
        const void* copyMemoryToUniformBufferSrcHostPointer;
        VkDeviceSize copyMemoryToUniformBufferSize;
        uint32_t vertexCount = 0;
    };

    bool draw(const draw_info&);

    void submit(renderer* renderer = nullptr);
}