# =================================================================================================
# Projeto: BSC (Bank Statement Conversion)
# Desenvolvido por Matheus Sousa, Laboratorio de Comunicacao e Seguranca da Informacao,
# Departamento de Eletronica e Sistemas, CTG-UFPE.
# =================================================================================================

cmake_minimum_required(VERSION 3.20)
project(BSC VERSION 2.5.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization flags for speed and legacy processors
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-O3 -Wall -Wextra -pthread)
endif()

# User sysroot paths for GLFW and X11
set(SYSROOT_DIR "$ENV{HOME}/.local/sysroot")
if(EXISTS "${SYSROOT_DIR}/usr/include")
    include_directories("${SYSROOT_DIR}/usr/include")
    link_directories("${SYSROOT_DIR}/usr/lib/x86_64-linux-gnu")
    set(CMAKE_INSTALL_RPATH "$ORIGIN;$ORIGIN/../lib;/usr/lib/x86_64-linux-gnu;/usr/lib64;/usr/lib;${SYSROOT_DIR}/usr/lib/x86_64-linux-gnu")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

# Project Include Directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/glfw/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/miniz
)

# Miniz C Library
add_library(miniz STATIC
    vendor/miniz/miniz.c
)
set_target_properties(miniz PROPERTIES C_STANDARD 99)

# Dear ImGui Library
add_library(imgui STATIC
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/backends/imgui_impl_glfw.cpp
    vendor/imgui/backends/imgui_impl_opengl3.cpp
)

# Core Engine Sources
set(CORE_SOURCES
    src/core/Types.cpp
    src/parsers/PdfParser.cpp
    src/parsers/OfxParser.cpp
    src/parsers/CsvParser.cpp
    src/parsers/XlsxParser.cpp
    src/parsers/QifParser.cpp
    src/encoders/OfxEncoder.cpp
    src/encoders/CsvEncoder.cpp
    src/encoders/XlsxEncoder.cpp
    src/encoders/QifEncoder.cpp
    src/encoders/PdfEncoder.cpp
    src/ocr/OcrEngine.cpp
    src/converter/ConverterEngine.cpp
)

add_library(BSCCore STATIC ${CORE_SOURCES})
target_link_libraries(BSCCore PUBLIC miniz)

# Find GUI dependencies
find_package(OpenGL REQUIRED)

set(GLFW_STATIC_LIB "${CMAKE_CURRENT_SOURCE_DIR}/vendor/glfw/lib/libglfw3.a")

# Executable: BSC (Main GUI & Headless Batch Application)
add_executable(BSC
    src/main.cpp
    src/ui/UIController.cpp
    src/ui/AppWindow.cpp
)

target_link_libraries(BSC PRIVATE
    BSCCore
    imgui
    OpenGL::GL
    ${GLFW_STATIC_LIB}
    X11
    pthread
    ${CMAKE_DL_LIBS}
    m
)

# Test Suite Executable
add_executable(test_converters
    tests/test_converters.cpp
)

target_link_libraries(test_converters PRIVATE
    BSCCore
    pthread
)

enable_testing()
add_test(NAME UnitTests COMMAND test_converters)
