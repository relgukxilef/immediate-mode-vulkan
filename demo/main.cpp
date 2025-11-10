#include <iostream>
#include <cassert>
#include <memory>

#define GLFW_INCLUDE_VULKAN
#define GLFW_VULKAN_STATIC
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <immediate_mode_vulkan/resources/vulkan_resources.h>
#include <immediate_mode_vulkan/draw.h>


using std::unique_ptr;
using std::out_ptr;

VkSampleCountFlagBits max_sample_count;

void glfw_check(int code) {
    if (code == GLFW_TRUE) {
        return;
    } else {
        throw std::runtime_error("Failed to initialize GLFW");
    }
}

struct unique_glfw {
    unique_glfw() { glfw_check(glfwInit()); }
    ~unique_glfw() { glfwTerminate(); }
};

struct glfw_window_deleter {
    typedef GLFWwindow* pointer;
    void operator()(GLFWwindow *window) {
        glfwDestroyWindow(window);
    }
};

using unique_window = unique_ptr<GLFWwindow, glfw_window_deleter>;

int main() {
    unique_glfw glfw;

    int window_width = 1280, window_height = 720;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    unique_window window{glfwCreateWindow(
        window_width, window_height, "Vulkan Experiments", nullptr, nullptr
    )};

    VkApplicationInfo application_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Vulkan Experiments",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0
    };

    // look up extensions needed by GLFW
    uint32_t glfw_extension_count = 0;
    auto glfw_extensions =
        glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    // loop up supported extensions
    uint32_t supported_extension_count = 0;
    vkEnumerateInstanceExtensionProperties(
        nullptr, &supported_extension_count, nullptr
    );
    auto supported_extensions =
        std::make_unique<VkExtensionProperties[]>(supported_extension_count);
    vkEnumerateInstanceExtensionProperties(
        nullptr, &supported_extension_count, supported_extensions.get()
    );

    // create instance
    auto extension_count = glfw_extension_count;
    auto extensions = std::make_unique<const char*[]>(extension_count);
    std::copy(
        glfw_extensions, glfw_extensions + glfw_extension_count,
        extensions.get()
    );
    imv::unique_instance instance;
    {
        VkInstanceCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = nullptr,
            .pApplicationInfo = &application_info,
            .enabledExtensionCount = static_cast<uint32_t>(extension_count),
            .ppEnabledExtensionNames = extensions.get(),
        };
        imv::check(vkCreateInstance(
            &createInfo, nullptr, out_ptr(instance)
        ));
    }
    imv::current_instance = instance.get();

    // create surface
    imv::unique_surface surface;
    imv::check(glfwCreateWindowSurface(
        instance.get(), window.get(), nullptr, out_ptr(surface)
    ));

    // look for available devices
    VkPhysicalDevice physical_device;

    uint32_t device_count = 0;
    imv::check(vkEnumeratePhysicalDevices(
        instance.get(), &device_count, nullptr
    ));
    if (device_count == 0) {
        throw std::runtime_error("no Vulkan capable GPU found");
    }
    {
        auto devices = std::make_unique<VkPhysicalDevice[]>(device_count);
        imv::check(vkEnumeratePhysicalDevices(
            instance.get(), &device_count, devices.get()
        ));
        // TODO: check for VK_KHR_swapchain support
        physical_device = devices[0]; // just pick the first one for now
    }

    // get properties of physical device
    max_sample_count = VK_SAMPLE_COUNT_1_BIT;
    {
        VkPhysicalDeviceProperties physical_device_properties;
        vkGetPhysicalDeviceProperties(
            physical_device, &physical_device_properties
        );

        VkSampleCountFlags sample_count_falgs =
            physical_device_properties.limits.framebufferColorSampleCounts &
            physical_device_properties.limits.framebufferDepthSampleCounts &
            physical_device_properties.limits.framebufferStencilSampleCounts;

        for (auto bit : {
            VK_SAMPLE_COUNT_64_BIT, VK_SAMPLE_COUNT_32_BIT,
            VK_SAMPLE_COUNT_16_BIT, VK_SAMPLE_COUNT_8_BIT,
            VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_2_BIT,
        }) {
            if (sample_count_falgs & bit) {
                max_sample_count = bit;
                break;
            }
        }
    }
    
    imv::renderer r(instance.get(), physical_device, surface.get());
    imv::global_renderer = &r;
    
    while (!glfwWindowShouldClose(window.get())) {
        imv::wait_frame();

        struct {
            float time;
        } uniforms;

        uniforms.time = float(glfwGetTime());

        for (auto i = 0u; i < 1000; i++) {
            imv::draw({
                .stages = {
                    { 
                        .code_file_name = "demo/vertex.glsl.spv",
                        .info = { .stage = VK_SHADER_STAGE_VERTEX_BIT, }
                    }, { 
                        .code_file_name = "demo/fragment.glsl.spv",
                        .info = { .stage = VK_SHADER_STAGE_FRAGMENT_BIT, }
                    }, 
                },
                .images = {
                    {
                        .file_name = "demo/placeholder.ktx",
                        .sampler_info = {
                            .magFilter = VK_FILTER_LINEAR,
                            .minFilter = VK_FILTER_LINEAR,
                            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                            .anisotropyEnable = VK_FALSE,
                            .minLod = 0.0,
                            .maxLod = VK_LOD_CLAMP_NONE,
                        }
                    }
                },
                .uniform_source_pointer = &uniforms,
                .uniform_source_size = sizeof(uniforms),
                .vertex_count = 4,
            });
            
            uniforms.time += 0.5f;
        }

        imv::submit();
        
        glfwPollEvents();
    }

    return 0;
}
