cmake_minimum_required(VERSION 3.5)

project(raytrace LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

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

find_package(Vulkan 1.1)

add_executable(
    raytrace
    main.cpp
    resources/vulkan_resources.cpp
    resources/vulkan_resources.h
    ui/ui.cpp ui/ui.h
)

target_link_libraries(
    raytrace
    gdi32 user32 kernel32 Vulkan::Vulkan glfw Vulkan::Headers glm
)

target_include_directories(
    raytrace PRIVATE 
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

add_shader(raytrace ui/video_vertex.glsl)
add_shader(raytrace ui/video_fragment.glsl)

target_compile_options(
    raytrace PUBLIC
    -Wall
    $<$<CONFIG:Debug>:-g -O0 -D_FORTIFY_SOURCE=2 /fsanitize=address>
)
target_link_options(
    raytrace PUBLIC
    $<$<CONFIG:Debug>:-g -O0 -D_FORTIFY_SOURCE=2>
)
