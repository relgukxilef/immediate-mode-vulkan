#include "main.h"

#include <coroutine>

#define GLFW_INCLUDE_VULKAN
#define GLFW_VULKAN_STATIC
#include <GLFW/glfw3.h>

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <vulkangl\vulkangl.h>
#include <ktx.h>

VkResult glfwCreateWindowSurface(
    VkInstance instance,
    GLFWwindow* handle,
    const VkAllocationCallbacks* allocator,
    VkSurfaceKHR* surface
) {
    // This function can't be provided by Emscripten, because it doesn't know
    // about VulkanGL
    *surface = vglCreateSurfaceForGL();
    return VK_SUCCESS;
}

GLFWAPI const char** glfwGetRequiredInstanceExtensions(uint32_t* count) {
    count = 0;
    return nullptr;
}

extern "C" KTX_error_code ktxVulkanDeviceInfo_Construct(
    struct ktxVulkanDeviceInfo* This,
    VkPhysicalDevice physicalDevice, VkDevice device,
    VkQueue queue, VkCommandPool cmdPool,
    const VkAllocationCallbacks* pAllocator
) {
   return KTX_SUCCESS;
}

extern "C" void ktxVulkanDeviceInfo_Destruct(
    struct ktxVulkanDeviceInfo* This
) {
}

extern "C" KTX_error_code ktxTexture2_VkUploadEx(
    ktxTexture2* This, ktxVulkanDeviceInfo* vdi,
    struct ktxVulkanTexture* vkTexture,
    VkImageTiling tiling,
    VkImageUsageFlags usageFlags,
    VkImageLayout finalLayout
) {
    return KTX_SUCCESS;
}

EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context;

bool awaitable::await_ready() { return false; }
void awaitable::await_suspend(std::coroutine_handle<> h) {
    emscripten_request_animation_frame(
        [](double, void* h) {
            std::coroutine_handle<>::from_address(h).resume();
            return true;
        }, h.address()
    );
}
void awaitable::await_resume() {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    vglSetCurrentSurfaceExtent({(uint32_t)width, (uint32_t)height});
}

awaitable animation_frame(GLFWwindow *window) {
    return awaitable{window};
}

const int glfw_api = GLFW_OPENGL_API;

task main_task;

task game_main();

int main() {
    main_task = game_main();
}
