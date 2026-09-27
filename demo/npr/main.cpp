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


    const float vertices[] = {
        0, 0, 0, 0,
        1, 0, 0, 0,
        0, 1, 0, 0,
        1, 1, 0, 0,
        0, 0, 1, 0,
        1, 0, 1, 0,
        0, 1, 1, 0,
        1, 1, 1, 0,
    };

    const unsigned faces[] = {
        0, 1, 2, 3, // bottom
        7,    // half front
        1, 5, // right
        0, 4, // back
        2, 6, // left
        7,    // half front
        4, 5, // top
    };

    // TODO: Add edge tangents to face-vertices
    // maybe processing faces in triangle strips is efficient
    // Visibility of a face has to be decided consistently on each vertex 
    // individually. Maybe store a vec4 with face normal and dot product with 
    // face provoking vertex at each face-vertex.
    // TODO: How to calculate edge tangents and face normals under amateur 
    // transformation?
    // TODO: It's not enough to store two edges per face-vertex. All edges 
    // adjacent to a vertex could be outlines, and the displacement depends on 
    // both.
    // Maybe determine outline edges and then iteratively inset shape in 
    // surface space. Maybe calculate a signed distance field in a surface 
    // texture or along the mesh. 
    // TODO: How to clip face insets? Maybe by face bounds? 
    // TODO: Create faces for edges
    // TODO: Create faces for vertices

    vector<float> face_vertices;

    for (unsigned face : faces) {
        face_vertices.insert(
            face_vertices.end(), 
            begin(vertices) + face * 4,
            begin(vertices) + (face + 1) * 4
        );
    }

    vector<unsigned> vertex_edge_neighbors_size(size(vertices) / 4);
    vector<unsigned> vertex_edge_neighbors_begin(size(vertices) / 4);
    vector<unsigned> vertex_edge_neighbors_vertices(size(faces));

    for (unsigned face : faces) {
        vertex_edge_neighbors_size[face]++;
    }

    {
        unsigned begin = 0;
        for (auto i = 0; i < size(vertices) / 4; i++) {
            vertex_edge_neighbors_begin[i] = begin;
            begin += vertex_edge_neighbors_size[i];
        }
    }

    fill(
        vertex_edge_neighbors_size.begin(), vertex_edge_neighbors_size.end(), 0
    );

    for (auto i = 0; i < size(faces); i++) {
        unsigned face = faces[i];
        unsigned begin = vertex_edge_neighbors_begin[face];
        unsigned &index = vertex_edge_neighbors_size[face];
        vertex_edge_neighbors_vertices[begin + index] = i;
        index++;
    }

    vector<float> vertex_edge_distance(size(vertices) / 4);

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

        // initialize
        for (auto i = 0; i < size(vertices) / 4; i++) {
            float distance = 1e6;
            auto vertex = std::begin(vertices) + i * 4;
            vec4 center = 
                view_matrix * vec4{*vertex++, *vertex++, *vertex++, 1,};
            unsigned begin = vertex_edge_neighbors_begin[i];
            bool front = false, back = false;
            for (auto j = 0; j < vertex_edge_neighbors_size[i]; j++) {
                auto strip = vertex_edge_neighbors_vertices[begin + j];
                int offsets[] = {-2, -1, 1, 2};
                vec4 next = {};
                for (int k = 0; k < 4; k++) {
                    int face = strip + offsets[k];
                    if (face < 0 || face >= size(faces))
                        continue;
                    auto vertex = std::begin(vertices) + faces[face] * 4;
                    vec4 position = next;
                    next = 
                        view_matrix * vec4{*vertex++, *vertex++, *vertex++, 1};
                    next = next / next.w - center / center.w;

                    if (position == vec4{})
                        continue;

                    vec3 normal = cross(vec3(position), vec3(next));
                    if (strip % 2)
                        // TODO: how does this interact with restart indices?
                        normal = -normal;
                    if (normal.z < 0)
                        front = true;
                    else
                        back = true;
                }
            }
            if (front && back)
                distance = 0;
            vertex_edge_distance[i] = distance;
        }

        // propagate
        // TODO: loop
        for (auto i = 0; i < size(vertices) / 4; i++) {
            float distance = vertex_edge_distance[i];
            auto vertex = std::begin(vertices) + i * 4;
            vec4 center = 
                view_matrix * vec4{*vertex++, *vertex++, *vertex++, 1,};
            unsigned begin = vertex_edge_neighbors_begin[i];
            for (auto j = 0; j < vertex_edge_neighbors_size[i]; j++) {
                auto strip = vertex_edge_neighbors_vertices[begin + j];
                int offsets[] = {-2, -1, 1, 2};
                for (int k = 0; k < 4; k++) {
                    int face = strip + offsets[k];
                    if (face < 0 || face >= size(faces))
                        continue;
                    auto vertex = std::begin(vertices) + faces[face] * 4;
                    vec4 position = 
                        view_matrix * vec4{*vertex++, *vertex++, *vertex++, 1};
                    position = position / position.w - center / center.w;

                    float length = glm::length(vec2(position));
                    distance = std::min(
                        distance,
                        vertex_edge_distance[faces[face]] + length
                    );
                    // TODO: account for paths across the face
                }
            }
            vertex_edge_distance[i] = distance;
        }

        // TODO: pass distances to vertex shader and apply threshold
        for (int i = 0; i < size(faces); i++) {
            face_vertices[i * 4 + 3] = vertex_edge_distance[faces[i]];
        }

        // TODO: depth buffer
        // TODO: indexed draw, only then it's possible to restart strips
        unsigned stride = sizeof(float) * 4;
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
                            .stride = stride,
                            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                        }, 
                        .attributes = {
                            { 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, },
                            { 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, },
                            { 2, stride, VK_FORMAT_R32G32B32A32_SFLOAT, },
                            { 3, 2 * stride, VK_FORMAT_R32G32B32A32_SFLOAT, },
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
            .vertex_count = size(faces),
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        });

        imv::submit();

        glfwPollEvents();
    }
}
