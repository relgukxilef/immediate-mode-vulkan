#include <iostream>
#include <cassert>
#include <memory>

#define GLFW_INCLUDE_VULKAN
#define GLFW_VULKAN_STATIC
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "resources/vulkan_resources.h"
#include "ui/ui.h"

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
        window_width, window_height, "Refdraw", nullptr, nullptr
    )};

    VkApplicationInfo application_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Refdraw",
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

    // check support for layers
    const char* enabled_layers[]{
        "VK_LAYER_KHRONOS_validation",
    };

    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    auto layers = std::make_unique<VkLayerProperties[]>(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, layers.get());
    for (const char* enabled_layer : enabled_layers) {
        bool supported = false;
        for (auto i = 0u; i < layer_count; i++) {
            auto equal = strcmp(enabled_layer, layers[i].layerName);
            if (equal == 0) {
                supported = true;
                break;
            }
        }
        if (!supported) {
            throw std::runtime_error("enabled layer not supported");
        }
    }

    // create instance
    const char *required_extensions[]{
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };
    auto extension_count =
        std::size(required_extensions) + glfw_extension_count;
    auto extensions = std::make_unique<const char*[]>(extension_count);
    std::copy(
        glfw_extensions, glfw_extensions + glfw_extension_count,
        extensions.get()
    );
    std::copy(
        required_extensions,
        required_extensions + std::size(required_extensions),
        extensions.get() + glfw_extension_count
    );
    unique_instance instance;
    {
        VkInstanceCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = nullptr,
            .pApplicationInfo = &application_info,
            .enabledLayerCount = std::size(enabled_layers),
            .ppEnabledLayerNames = enabled_layers,
            .enabledExtensionCount = static_cast<uint32_t>(extension_count),
            .ppEnabledExtensionNames = extensions.get(),
        };
        check(vkCreateInstance(
            &createInfo, nullptr, out_ptr(instance)
        ));
    }
    current_instance = instance.get();

    // create surface
    unique_surface surface;
    check(glfwCreateWindowSurface(
        instance.get(), window.get(), nullptr, out_ptr(surface)
    ));

    // look for available devices
    VkPhysicalDevice physical_device;

    uint32_t device_count = 0;
    check(vkEnumeratePhysicalDevices(instance.get(), &device_count, nullptr));
    if (device_count == 0) {
        throw std::runtime_error("no Vulkan capable GPU found");
    }
    {
        auto devices = std::make_unique<VkPhysicalDevice[]>(device_count);
        check(vkEnumeratePhysicalDevices(
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
    
    ::ui ui(physical_device, surface.get());
    
    while (!glfwWindowShouldClose(window.get())) {
        ui.render();
        
        glfwPollEvents();
    }

    return 0;
}
