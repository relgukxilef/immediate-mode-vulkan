#include <vulkan/vulkan.h>
#include <cstdint>
#include <initializer_list>
#include <memory>

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

struct draw_info {
    renderer* renderer = nullptr;
    bool prepare_only = false;
    const VkDescriptorSetLayoutCreateInfo* descriptor_set_layout;
    std::initializer_list<const char*> shaderModuleCodeFileNames;
    const void* copyMemoryToUniformBufferSrcHostPointer;
    VkDeviceSize copyMemoryToUniformBufferSize;
    uint32_t vertexCount = 0;
};

bool draw(const draw_info&);

void submit(renderer* renderer = nullptr);
