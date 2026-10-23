add_executable(demo-planets main.cpp)
target_link_libraries(demo-planets PRIVATE ImmediateModeVulkan)
target_compile_features(demo-planets PRIVATE cxx_std_23)

add_shader(demo-planets planet_fragment.glsl)
add_shader(demo-planets planet_vertex.glsl)
