cmake_minimum_required(VERSION 3.5)

project(ImmediateModeVulkan LANGUAGES C CXX)

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if (NOT EMSCRIPTEN)
    set(DESKTOP 1)
else()
    set(DESKTOP 0)
endif()

if(NOT CMAKE_SIZEOF_VOID_P)
    # Set a default value for CMAKE_SIZEOF_VOID_P
    # It is not set when building with emscripten
    set(CMAKE_SIZEOF_VOID_P 4)
endif()

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

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)

FetchContent_Declare(
    VulkanHeaders
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG v1.4.307
)

FetchContent_Declare(
    VulkanMemoryAllocator
    GIT_REPOSITORY 
        https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.3.0
)

FetchContent_Declare(
    emsdk
    GIT_REPOSITORY https://github.com/emscripten-core/emsdk.git
    GIT_TAG 5.0.5
)

FetchContent_Declare(
    VulkanGL
    GIT_REPOSITORY https://github.com/relgukxilef/VulkanGL.git
    GIT_TAG main
)

FetchContent_Declare(
    freetype
    GIT_REPOSITORY https://github.com/freetype/freetype.git
    GIT_TAG VER-2-14-3
)

FetchContent_MakeAvailable(
    glm VulkanHeaders VulkanMemoryAllocator emsdk VulkanGL freetype
)

set(KTX_FEATURE_TESTS OFF)
set(KTX_FEATURE_TOOLS ON)
set(KTX_FEATURE_VK_UPLOAD ON)
# Unfortunately, KTX can't be tricked into emitting Vulkan calls when 
# __EMSCRIPTEN__ is defined, and that can't be undefined
FetchContent_Declare(
    KTX
    GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
    GIT_TAG v4.4.2
)
FetchContent_MakeAvailable(KTX)

if (DESKTOP)
    FetchContent_MakeAvailable(glfw)
endif()

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
    $<${DESKTOP}:gdi32 user32 kernel32 Vulkan::Vulkan glfw>
    Vulkan::Headers glm
    VulkanMemoryAllocator ktx_read
)

target_include_directories(
    ImmediateModeVulkan PUBLIC include
)

target_compile_features(ImmediateModeVulkan PRIVATE cxx_std_23)

# TODO: Move demo to separate CMakeLists.txt
add_executable(
    demo
    demo/main.cpp
    $<IF:$<BOOL:${EMSCRIPTEN}>,demo/main-emscripten.cpp,demo/main-desktop.cpp>
)

target_compile_features(demo PRIVATE cxx_std_23)

target_link_libraries(
    demo PRIVATE 
    ImmediateModeVulkan
    freetype
)

target_include_directories(
    demo PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if (EMSCRIPTEN)
    #target_compile_options(ImmediateModeVulkan PRIVATE -fwasm-exceptions)
    target_link_options(demo PRIVATE -sUSE_GLFW=3 -sMIN_WEBGL_VERSION=2)
    target_link_libraries(demo PRIVATE VulkanGL::VulkanGL)
    set_target_properties(demo PROPERTIES SUFFIX ".html")
    target_link_options(
        demo PUBLIC 
        -gsource-map=inline 
        --preload-file "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/demo@demo"
    )
    target_compile_options(demo PUBLIC -gsource-map=inline)
endif()

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
    target_sources(${TARGET} PRIVATE ${output_path}.spv ${input_path})
endfunction(add_shader)

add_shader(demo demo/vertex.glsl)
add_shader(demo demo/fragment.glsl)
add_shader(demo demo/flat_fragment.glsl)
add_shader(demo demo/text_fragment.glsl)

function(add_texture TARGET TEXTURE)
    add_custom_command(
        OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TEXTURE}.ktx
        COMMAND 
            ktx create --format R8G8B8A8_SRGB --encode uastc --generate-mipmap
            ${CMAKE_CURRENT_SOURCE_DIR}/${TEXTURE}
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TEXTURE}.ktx
        # Unfortunately, ktx doesn't update the modification date at the end
        COMMAND 
            ${CMAKE_COMMAND} -E touch
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TEXTURE}.ktx
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${TEXTURE} ktx
        COMMENT "Compress image"
    )
    target_sources(
        ${TARGET} PRIVATE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TEXTURE}.ktx
    )
endfunction()

add_texture(demo demo/1.png)
add_texture(demo demo/2.png)

add_custom_command(
    TARGET demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/demo/daggersquare.regular.otf
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/demo/daggersquare.regular.otf
)
