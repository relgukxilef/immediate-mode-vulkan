#include "glm/ext/matrix_transform.hpp"
#include <cassert>
#include <memory>
#include <numbers>
#include <vector>

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

struct input {
    float steering = 0, acceleration = 0;
};

const float time_delta = 0.5e-2f;

vec2 clamp_length(vec2 x, float max) {
    float length_squared = glm::dot(x, x);
    if (length_squared < max * max)
        return x;
    return x * glm::inversesqrt(length_squared) * max;
}

vec2 project(vec2 x, vec2 target) {
    return target * glm::dot(x, target);
}

float move_towards(float x, float target, float distance) {
    return x + clamp(target - x, -distance, distance);
}

float steering_speed = 0.5f;
float turning_speed = 0.04f;
float acceleration = 40.0;
float camera_speed = 10;
float camera_acceleration = 0;

float line_side(vec2 a, vec2 b, vec2 point) {
    b -= a;
    point -= a;
    return dot(vec2{b.y, -b.x}, point);
}

float line_distance(vec2 a, vec2 b, vec2 point) {
    b -= a;
    point -= a;
    return line_side({}, normalize(b), point);
}

vec2 line_collide(vec2 a, vec2 b, vec2 point, float depth) {
    b -= a;
    vec2 local = point - a;
    vec2 tangent = normalize(b);
    vec2 normal = {tangent.y, -tangent.x};
    float t = dot(tangent, local);
    if (t < 0)
        return point;
    if (t > dot(tangent, b))
        return point;
    float s = dot(normal, local);
    if (s < 0)
        return point;
    if (s > depth)
        return point;

    return a + tangent * t;
}

struct track {
    std::vector<vec2> strip;
    vec2 end = {}, forward = {0, 1};
    void append_straight(float length, float width) {
        vec2 normal = {forward.y, -forward.x};
        strip.push_back(end + width * normal);
        strip.push_back(end - width * normal);
        end += forward * length;
    }
    void append_turn(float radius, float width) {
        int resolution = 8;
        mat2 rotation = rotate(
            mat4(1), pi<float>() / resolution * sign(radius) / 2, 
            {0, 0, 1}
        );
        vec2 normal = {forward.y, -forward.x};
        vec2 left = (radius + width) * normal, right = (radius - width) * normal;
        vec2 center = end - normal * radius;
        for (auto i = 0u; i < resolution; i++) {
            strip.push_back(center + left);
            strip.push_back(center + right);
            left = rotation * left;
            right = rotation * right;
        }
        end = center + forward * abs(radius);
        forward = -normal * sign(radius);
    }
    vec2 collide(vec2 point) {
        vec2 a = strip[0], b = strip[1];
        for (int i = 2; i < strip.size(); i+=2) {
            vec2 c = strip[i], d = strip[i + 1];

            point = line_collide(a, c, point, 4);
            point = line_collide(d, b, point, 4);

            a = c;
            b = d;
        }
        return point;
    }
};

struct car {
    vec2 position = {};
    vec2 velocity = {};
    float heading = 0.f;
    float steering = 0.f;
    
    void update(input input, ::track& track) {
        input.steering = glm::clamp(input.steering, -1.f, 1.f);
        input.acceleration = glm::clamp(input.acceleration, -1.f, 1.f);

        float speed = glm::length(velocity);

        steering = move_towards(
            steering, input.steering, speed * time_delta * steering_speed
        );
        
        float heading_change = 
            speed * time_delta * steering * turning_speed;
        heading -= heading_change;

        vec2 forward = { sin(heading), cos(heading) };

        velocity = project(velocity, forward);

        velocity = 
            mat2(rotate(mat4(1.0), heading_change, {0, 0, 1})) * velocity;
        
        float forward_speed = glm::dot(velocity, forward);

        if (input.acceleration < 0 && forward_speed > 0)
            input.acceleration *= 8;

        velocity += 
            acceleration * time_delta / 
            (1.f + speed) * input.acceleration * forward * 4.f;

        position += velocity * time_delta;

        vec2 sum_push = {};
        vec2 sum_rotation = {};
        vec2 corners[] = {
            {-1, -2}, {1, -2}, {1, 2}, {-1, 2}, 
        };
        for (auto corner : corners) {
            corner = 
                corner.x * vec2{forward.y, -forward.x} + corner.y * forward;
            corner += position;
            vec2 destination = track.collide(corner);
            sum_push += destination - corner;
            corner -= position;
            destination -= position;
            vec2 relative = {
                dot({corner.y, -corner.x}, destination),
                dot(corner, destination), 
            };
            sum_rotation += relative;
        }

        position += sum_push / 4.f;
        heading += atan2(sum_rotation.x, sum_rotation.y);
    }
};

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

    imv::renderer r(instance.get(), surface.get());
    imv::global_renderer = &r;

    track track;

    track.append_straight(80, 20);
    track.append_turn(40, 20);
    track.append_straight(40, 20);
    track.append_turn(-40, 20);
    track.append_straight(40, 20);
    track.append_straight(10, 10);
    track.append_turn(-40, 10);
    track.append_straight(10, 10);
    track.append_straight(150, 20);
    track.append_turn(-80, 20);
    track.append_straight(170, 20);
    track.append_turn(-80, 20);
    track.append_turn(-80, 20);
    track.append_straight(0, 20);

    car car;
    vec2 camera_position = {}, camera_velocity = {};
    float last_update = glfwGetTime();

    while (!glfwWindowShouldClose(window.get())) {
        imv::wait_frame();

        input input;
        if (glfwGetKey(window.get(), GLFW_KEY_UP))
            input.acceleration++;
        if (glfwGetKey(window.get(), GLFW_KEY_DOWN))
            input.acceleration--;
        if (glfwGetKey(window.get(), GLFW_KEY_RIGHT))
            input.steering++;
        if (glfwGetKey(window.get(), GLFW_KEY_LEFT))
            input.steering--;

        while (last_update < glfwGetTime()) {
            last_update += time_delta;
            car.update(input, track);

            vec2 forward = { sin(car.heading), cos(car.heading) };

            camera_position += camera_velocity * time_delta;
            camera_position += (
                car.position - forward * 10.f - camera_position
            ) * time_delta * camera_speed;
            camera_velocity += 
                time_delta * camera_acceleration * 
                (car.velocity - camera_velocity);
        }

        int width, height;
        glfwGetWindowSize(window.get(), &width, &height);

        mat4 view_matrix = 
            glm::infinitePerspective(1.5f, (float)width / height, 0.1f) *
            glm::lookAt(
                vec3{camera_position, 10}, vec3(car.position, 0), 
                vec3{0, 0, -1}
            );

        struct uniforms_t {
            mat4 matrix;
            vec4 colors;
        };
        
        vec2 positions[] = { // and texture coordinates
            vec2(-1, -1), vec2(0, 0),
            vec2(1, -1), vec2(1, 0),
            vec2(-1, 1), vec2(0, 1),
            vec2(1, 1), vec2(1, 1),
        };

        auto stages = {
            imv::stage_info{ 
                .code_file_name = "demo/vertex.glsl.spv",
                .info = { .stage = VK_SHADER_STAGE_VERTEX_BIT, }
            }, { 
                .code_file_name = "demo/fragment.glsl.spv",
                .info = { .stage = VK_SHADER_STAGE_FRAGMENT_BIT, }
            }, 
        };

        auto bindings = {
            imv::vertex_binding_info{
                .buffer_source_pointer = &positions,
                .buffer_source_size = sizeof(positions),
                .description = {
                    .stride = 2 * sizeof(vec2),
                    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                }, 
                .attributes = {
                    { 0, 0, VK_FORMAT_R32G32_SFLOAT, },
                    { 1, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(vec2) },
                },
            },
        };

        mat4 model_matrix = glm::scale(mat4(1.f), vec3(1, 1, 1));

        uniforms_t uniforms{
            .matrix = view_matrix * model_matrix,
            .colors = vec4(1),
        };

        imv::draw({
            .stages = stages,
            .vertex_input_bindings = {
                {
                    .buffer_source_pointer = track.strip.data(),
                    .buffer_source_size = sizeof(vec2) * track.strip.size(),
                    .description = {
                        .stride = sizeof(vec2),
                        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                    }, 
                    .attributes = {
                        { 0, 0, VK_FORMAT_R32G32_SFLOAT, },
                        { 1, 0, VK_FORMAT_R32G32_SFLOAT, },
                    },
                },
            },
            .uniform_source_pointer = &uniforms,
            .uniform_source_size = sizeof(uniforms),
            .vertex_count = (uint32_t)track.strip.size(),
        });

        model_matrix = glm::scale(
            glm::rotate(
                glm::translate(mat4(1.0), vec3(car.position, 0)), 
                -car.heading, vec3{0, 0, 1}
            ), 
            vec3(1, 2, 1)
        );
        
        uniforms = {
            .matrix = view_matrix * model_matrix,
            .colors = vec4(0.5),
        };

        imv::draw({
            .stages = stages,
            .vertex_input_bindings = bindings,
            .uniform_source_pointer = &uniforms,
            .uniform_source_size = sizeof(uniforms),
            .vertex_count = 4,
        });
        
        imv::submit();
        
        glfwPollEvents();
    }

    return 0;
}
