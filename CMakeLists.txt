cmake_minimum_required(VERSION 3.22)
project(hexagon_renderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# User-local prefix holding extracted Ubuntu packages (glslang-tools, libglfw3-dev,
# vulkan-validationlayers) when they are not installed system-wide. See docs/ENVIRONMENT.md.
set(HEX_DEPS_PREFIX "$ENV{HOME}/.local/hexdeps/root/usr" CACHE PATH "Optional local dependency prefix")
list(APPEND CMAKE_PREFIX_PATH "${HEX_DEPS_PREFIX}" "${HEX_DEPS_PREFIX}/lib/aarch64-linux-gnu/cmake")

option(HEX_BUILD_VIEWER "Build the GLFW windowed viewer" ON)
option(HEX_BUILD_TESTS "Build tests" ON)

find_package(Vulkan REQUIRED)
find_program(GLSLANG_VALIDATOR glslangValidator HINTS "${HEX_DEPS_PREFIX}/bin" REQUIRED)

add_compile_options(-Wall -Wextra -Wpedantic -Wno-missing-field-initializers)

# ---- Shaders -> SPIR-V, embedded into the binary as uint32 arrays ----
set(SHADER_SOURCES
  shaders/test_pattern.vert shaders/test_pattern.frag
  shaders/mesh_attr.vert shaders/mesh_pos.vert
  shaders/mesh_color.frag shaders/mesh_id.frag shaders/mesh_count.frag)
set(SHADER_HEADERS)
foreach(src ${SHADER_SOURCES})
  get_filename_component(name ${src} NAME)
  string(REPLACE "." "_" var ${name})
  set(out ${CMAKE_BINARY_DIR}/shaders/${var}.h)
  add_custom_command(OUTPUT ${out}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
    COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.2 --vn spv_${var} -o ${out} ${CMAKE_SOURCE_DIR}/${src}
    DEPENDS ${CMAKE_SOURCE_DIR}/${src} ${CMAKE_SOURCE_DIR}/shaders/common.glsl
    COMMENT "glslang ${name}")
  list(APPEND SHADER_HEADERS ${out})
endforeach()
add_custom_target(shaders DEPENDS ${SHADER_HEADERS})

# ---- Core library (GPU-independent) ----
add_library(hexcore STATIC
  src/geometry/grid.cpp
  src/geometry/mesh.cpp
  src/ink_model/ink_model.cpp
  src/ink_model/svg.cpp
  src/platform/cli.cpp
  src/platform/sysinfo.cpp
  src/platform/json.cpp)
target_include_directories(hexcore PUBLIC src)

# ---- Vulkan library ----
add_library(hexvk STATIC
  src/vk/context.cpp
  src/vk/offscreen.cpp
  src/vk/pipeline.cpp
  src/renderers/test_pattern.cpp
  src/renderers/mesh_renderer.cpp
  src/renderers/validation.cpp)
add_dependencies(hexvk shaders)
target_include_directories(hexvk PUBLIC src ${CMAKE_BINARY_DIR}/shaders)
target_link_libraries(hexvk PUBLIC hexcore Vulkan::Vulkan)

add_executable(hexbench src/app/hexbench.cpp)
target_link_libraries(hexbench PRIVATE hexvk)

if(HEX_BUILD_VIEWER)
  find_package(glfw3 3.3 REQUIRED)
  add_executable(hexview src/app/hexview.cpp src/vk/swapchain.cpp)
  target_link_libraries(hexview PRIVATE hexvk glfw)
  set_target_properties(hexview PROPERTIES BUILD_RPATH "${HEX_DEPS_PREFIX}/lib/aarch64-linux-gnu")
endif()

if(HEX_BUILD_TESTS)
  enable_testing()
  add_executable(hextests
    tests/main.cpp tests/test_grid.cpp tests/test_mesh.cpp tests/test_ink.cpp
    tests/test_cli.cpp tests/test_json.cpp)
  target_include_directories(hextests PRIVATE third_party/doctest)
  target_link_libraries(hextests PRIVATE hexcore)
  add_test(NAME unit COMMAND hextests)

  # GPU tests: render to offscreen targets and compare against CPU references.
  add_executable(hexgputests tests/main.cpp tests/test_gpu.cpp)
  target_include_directories(hexgputests PRIVATE third_party/doctest)
  target_link_libraries(hexgputests PRIVATE hexvk)
  add_test(NAME gpu COMMAND hexgputests)
  set_tests_properties(gpu PROPERTIES LABELS gpu
    ENVIRONMENT "VK_LAYER_PATH=${HEX_DEPS_PREFIX}/share/vulkan/explicit_layer.d;LD_LIBRARY_PATH=${HEX_DEPS_PREFIX}/lib/aarch64-linux-gnu;HEX_VALIDATION=1")
endif()
