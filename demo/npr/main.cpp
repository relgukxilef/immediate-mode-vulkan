#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "vulkan/vulkan_core.h"
#include <immediate_mode_vulkan/resources/vulkan_resources.h>
#include <immediate_mode_vulkan/draw.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <memory>
#include <vector>

using namespace std;
using namespace glm;

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

    // API depends on platform but must be set after call to glfwInit.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    unique_window window{glfwCreateWindow(
        window_width, window_height, "Vulkan Experiments", nullptr, nullptr
    )};

    VkApplicationInfo application_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Vulkan Experiments",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Immediate Mode Vulkan",
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

    imv::renderer renderer(instance.get(), surface.get());
    imv::global_renderer = &renderer;


    float vertices[] = {
        0, 0, 0, 0,
        1, 0, 0, 0,
        0, 1, 0, 0,
        1, 1, 0, 0,
        0, 0, 1, 0,
        1, 0, 1, 0,
        0, 1, 1, 0,
        1, 1, 1, 0,
    };

    unsigned faces[] = {
        0, 1, 2, 3, // bottom
        7,    // half front
        1, 5, // right
        0, 4, // back
        2, 6, // left
        7,    // half front
        4, 5, // top
    };
    vector<float> face_vertices;

    for (unsigned face : faces) {
        face_vertices.insert(
            face_vertices.end(), 
            begin(vertices) + face * 4,
            begin(vertices) + (face + 1) * 4
        );
    }
    

    while (!glfwWindowShouldClose(window.get())) {
        imv::wait_frame();

        int width, height;
        glfwGetWindowSize(window.get(), &width, &height);

        struct uniforms_t {
            mat4 matrix;
        };

        float time = glfwGetTime();
        mat4 view_matrix = 
            infinitePerspective(0.5f, (float)width / height, 0.1f) *
            lookAt(
                vec3{3, 3, 3}, vec3(0, 0, 0), 
                vec3{0, 0, -1}
            ) *
            rotate(mat4(1), time, {0, 0, 1}) *
            translate(mat4(1), vec3(-0.5));

        imv::draw({
            .stages = {
                {
                    .code_file_name = "main_vertex.glsl.spv",
                    .info = { .stage = VK_SHADER_STAGE_VERTEX_BIT, },
                }, {
                    .code_file_name = "main_fragment.glsl.spv",
                    .info = { .stage = VK_SHADER_STAGE_FRAGMENT_BIT, },
                },
            },
            .vertex_input_bindings = {
                {
                    imv::vertex_binding_info{
                        .buffer_source_pointer = face_vertices.data(),
                        .buffer_source_size = 
                            sizeof(float) * face_vertices.size(),
                        .description = {
                            .stride = sizeof(float) * 4,
                            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                        }, 
                        .attributes = {
                            { 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, },
                        },
                    }
                }
            },
            .uniform_source = uniforms_t{
                .matrix = view_matrix,
            },
            .vertex_count = size(faces),
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        });

        imv::submit();
        
        glfwPollEvents();
    }
}
