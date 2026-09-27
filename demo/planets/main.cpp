#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/constants.hpp"
#include "vulkan/vulkan_core.h"
#include <cmath>
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

struct tile {
    vec3 center;
    vec3 corners[4];
};

vec3 mix(vec3 a, vec3 b, float x) {
    return a * (1 - x) + b * x;
}

auto one_minus_cos(auto x) {
    x = sin(x * 0.5f);
    return x * x * 2.f;
}

vec3 get_arc_vertex(
    vec3 center, vec3 a, vec3 b, float angle, float parameter
) {
    vec3 normal = a - center;
    vec3 tangent = cross(normalize(cross(normal, b - center)), normal);
    // TODO: account for quantization error in b
    return 
        a + 
        tangent * sin(angle * parameter) - 
        normal * one_minus_cos(angle * parameter);
}

vec3 get_arc_vertex2(
    vec3 center, vec3 a, vec3 b, float angle, float parameter
) {
    // Law of sines
    // Can't avoid two divisions as only x / x is guaranteed to be 1.
    float divisor = sin(angle);
    float b_factor = sin(parameter * angle) / divisor;
    float a_factor = sin((1 - parameter) * angle) / divisor;
    return center * (1 - a_factor - b_factor) + a * a_factor + b * b_factor;
}

// Calculate vertex position from center and corners, relative to first corner.
// coordinates are position of tile in [0, 1]^2,
// size is a factor of pi over two
// parameter is point on tile in [0, 1]^2
vec3 get_geodesic_cube_vertex(
    vec3 center, vec3 corners[3], vec2 coordinates, float size, vec2 parameter
) {
    vec3 angles = 2.f * atan(sin(vec3(
        coordinates.y, coordinates.y + size, coordinates.x + parameter.x * size
    ) * half_pi<float>() + quarter_pi<float>()));

    vec3 a = get_arc_vertex2(center, {}, corners[0], angles.x, parameter.x);
    vec3 b = 
        get_arc_vertex2(center, corners[1], corners[2], angles.y, parameter.x);

    return get_arc_vertex2(center, a, b, angles.z, parameter.y);
}

vec3 get_geodesic_cube_vertex(vec3 center, ivec2 index, float size) {
    vec3 corners[3] = {{-1, 0, 0}, {0, -1, 0}, {-1, -1, 0}};
    return get_geodesic_cube_vertex(
        vec3{-.5f}, corners, {}, 1, 
        vec2(index) * size
    );
}

void push(vector<float>::iterator& value, vec3 point) {
    *(value++) = point.x;
    *(value++) = point.y;
    *(value++) = point.z;
}

void get_tile(
    vec3 center, vec3 cube, int level, int resolution, vector<float>& positions
) {
    int side = 1 << resolution;
    float step = 1.f / side;
    positions.resize((side + 2) * side * 2 * 3);
    auto value = positions.begin();
    for (int i = 0; i < side; i++) {
        push(value, get_geodesic_cube_vertex({}, {i, 0}, step));
        push(value, get_geodesic_cube_vertex({}, {i, 0}, step));
        push(value, get_geodesic_cube_vertex({}, {i + 1, 0}, step));
        for (int j = 0; j < side; j++) {
            push(value, get_geodesic_cube_vertex({}, {i, j + 1}, step));
            push(value, get_geodesic_cube_vertex({}, {i + 1, j + 1}, step));
        }
        push(value, get_geodesic_cube_vertex({}, {i + 1, side}, step));
    }
    assert(value == positions.end());
}

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

    vector<float> positions;
    get_tile({}, {}, 0, 6, positions);

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
                vec3{3}, vec3(-.5), 
                vec3{0, 0, -1}
            ) *
            rotate(mat4(1), time, {0, 0, 1});

        imv::draw({
            .stages = {
                {
                    .code_file_name = "planet_vertex.glsl.spv",
                    .info = { .stage = VK_SHADER_STAGE_VERTEX_BIT, },
                }, {
                    .code_file_name = "planet_fragment.glsl.spv",
                    .info = { .stage = VK_SHADER_STAGE_FRAGMENT_BIT, },
                },
            },
            .vertex_input_bindings = {
                {
                    imv::vertex_binding_info{
                        .buffer_source_pointer = positions.data(),
                        .buffer_source_size = 
                            sizeof(float) * positions.size(),
                        .description = {
                            .stride = 3 * sizeof(float),
                            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                        }, 
                        .attributes = {
                            { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, },
                        },
                    }
                }
            },
            .pipeline_rasterization_state = {
                .sType = 
                    VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_BACK_BIT,
                .lineWidth = 1.0f, 
            },
            .uniform_source = uniforms_t{
                .matrix = view_matrix,
            },
            .vertex_count = (unsigned)positions.size() / 3,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        });

        imv::submit();

        glfwPollEvents();
    }
}
