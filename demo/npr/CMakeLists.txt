add_executable(demo-npr main.cpp)
target_link_libraries(demo-npr PRIVATE ImmediateModeVulkan)
target_compile_features(demo-npr PRIVATE cxx_std_23)

add_shader(demo-npr main_fragment.glsl)
add_shader(demo-npr main_vertex.glsl)
