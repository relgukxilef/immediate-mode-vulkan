cmake_minimum_required(VERSION 3.5)

project(ImmediateModeVulkan LANGUAGES C CXX)

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if (CMAKE_BUILD_TYPE EQUAL "Debug")
    add_compile_options(/fsanitize=address)
    add_link_options(/fsanitize=address)
endif()

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

include(FetchContent)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
    VulkanHeaders
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG v1.4.307
)
FetchContent_MakeAvailable(VulkanHeaders)

FetchContent_Declare(
    VulkanMemoryAllocator
    GIT_REPOSITORY 
        https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.3.0
)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

set(KTX_FEATURE_TESTS OFF)
set(KTX_FEATURE_TOOLS ON)
set(KTX_FEATURE_VK_UPLOAD ON)
FetchContent_Declare(
    KTX
    GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
    GIT_TAG v4.4.2
)
FetchContent_MakeAvailable(KTX)

find_package(Vulkan 1.1)

add_library(
    ImmediateModeVulkan STATIC
    source/draw.cpp 
    include/immediate_mode_vulkan/draw.h
    source/resources/vulkan_resources.cpp
    include/immediate_mode_vulkan/resources/vulkan_resources.h
    source/resources/vulkan_memory_allocator_resource.cpp
    include/immediate_mode_vulkan/resources/vulkan_memory_allocator_resource.h
    include/immediate_mode_vulkan/resources/ktx_resources.h
)

target_link_libraries(
    ImmediateModeVulkan
    gdi32 user32 kernel32 Vulkan::Vulkan glfw Vulkan::Headers glm
    VulkanMemoryAllocator ktx_read
)

target_include_directories(
    ImmediateModeVulkan PUBLIC include
)

target_compile_features(ImmediateModeVulkan PRIVATE cxx_std_23)

add_executable(
    demo
    demo/main.cpp
)

target_compile_features(demo PRIVATE cxx_std_23)

target_link_libraries(
    demo PRIVATE ImmediateModeVulkan
)

target_include_directories(
    demo PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

function(add_shader TARGET SHADER)
    find_program(GLSLC glslc)

    set(input_path ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER})
    set(output_path ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${SHADER})

    get_filename_component(output_directory ${output_path} DIRECTORY)
    file(MAKE_DIRECTORY ${output_directory})

    add_custom_command(
        OUTPUT ${output_path}.spv
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        COMMAND 
        ${GLSLC} --target-env=vulkan1.1 -O -o ${SHADER}.spv ${input_path}
        DEPENDS ${input_path}
        VERBATIM
    )
    target_sources(
        ${TARGET} PRIVATE ${output_path}.spv ${input_path}
    )
endfunction(add_shader)

add_shader(demo demo/vertex.glsl)
add_shader(demo demo/fragment.glsl)

add_custom_command(
    OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/demo/placeholder.ktx
    COMMAND 
        ktx create --format R8G8B8A8_SRGB --encode uastc --generate-mipmap
        ${CMAKE_CURRENT_SOURCE_DIR}/demo/placeholder.png
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/demo/placeholder.ktx
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/demo/placeholder.png ktx
    COMMENT "Compressing image"
)
target_sources(
    demo PRIVATE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/demo/placeholder.ktx
)
