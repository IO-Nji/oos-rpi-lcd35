cmake_minimum_required(VERSION 3.16)
project(oos-rpi-lcd35 VERSION 0.9.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/core/CMakeLists.txt")
    message(FATAL_ERROR "The core submodule is missing. Run: git submodule update --init")
endif()

# -Wno-psabi: GCC notes about an old ARM calling-convention change are not about this code
add_compile_options(-Wall -Wextra -Wno-psabi)

# Drawing, data providers and pages, shared with oos-acemagic-s1
add_subdirectory(core)

add_executable(oos-rpi-lcd35
    src/main.cpp
    src/hw/console.cpp
    src/hw/fb_display.cpp
    src/hw/touch_device.cpp
)
target_include_directories(oos-rpi-lcd35 PRIVATE src)
target_compile_definitions(oos-rpi-lcd35 PRIVATE OOS_VERSION="${PROJECT_VERSION}")
target_link_libraries(oos-rpi-lcd35 PRIVATE oos_core)

# Offline renderer and its layout and contrast checks, run after every build
add_executable(oos-rpi-preview tools/preview.cpp)
target_include_directories(oos-rpi-preview PRIVATE src core/tools)
target_link_libraries(oos-rpi-preview PRIVATE oos_core)
add_custom_command(TARGET oos-rpi-preview POST_BUILD COMMAND oos-rpi-preview --check)

add_executable(oos-rpi-tests tests/test_main.cpp)
target_include_directories(oos-rpi-tests PRIVATE src)
target_link_libraries(oos-rpi-tests PRIVATE oos_core)
add_custom_command(TARGET oos-rpi-tests POST_BUILD COMMAND oos-rpi-tests)

enable_testing()
add_test(NAME unit COMMAND oos-rpi-tests)
add_test(NAME layout-and-contrast COMMAND oos-rpi-preview --check)
