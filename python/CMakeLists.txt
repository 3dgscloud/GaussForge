# Python binding CMakeLists.txt
# This file is included when building the Python package via scikit-build-core

cmake_minimum_required(VERSION 3.26)

project(gaussforge_python
    VERSION 0.4.2
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Python and nanobind
find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
find_package(nanobind CONFIG REQUIRED)

# Include parent project's dependencies
set(GAUSS_FORGE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

# Configure version header
configure_file(
    ${GAUSS_FORGE_ROOT}/include/gf/core/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/gf/core/version.h
    @ONLY
)

# Include dependency configurations
include(FetchContent)
include(${GAUSS_FORGE_ROOT}/cmakes/zlib.cmake)
include(${GAUSS_FORGE_ROOT}/cmakes/sog.cmake)
include(${GAUSS_FORGE_ROOT}/cmakes/spz.cmake)

# Core library source files
set(GAUSS_FORGE_SOURCES
    ${GAUSS_FORGE_ROOT}/src/core/model_info.cpp
    ${GAUSS_FORGE_ROOT}/src/core/validate.cpp
    ${GAUSS_FORGE_ROOT}/src/io/registry.cpp
    ${GAUSS_FORGE_ROOT}/src/io/ply_auto.cpp
    ${GAUSS_FORGE_ROOT}/src/io/ply_compressed_reader.cpp
    ${GAUSS_FORGE_ROOT}/src/io/ply_compressed_writer.cpp
    ${GAUSS_FORGE_ROOT}/src/io/ply_reader.cpp
    ${GAUSS_FORGE_ROOT}/src/io/ply_writer.cpp
    ${GAUSS_FORGE_ROOT}/src/io/splat_reader.cpp
    ${GAUSS_FORGE_ROOT}/src/io/splat_writer.cpp
    ${GAUSS_FORGE_ROOT}/src/io/ksplat_reader.cpp
    ${GAUSS_FORGE_ROOT}/src/io/ksplat_writer.cpp
    ${GAUSS_FORGE_ROOT}/src/io/spz_reader.cpp
    ${GAUSS_FORGE_ROOT}/src/io/spz_writer.cpp
    ${GAUSS_FORGE_ROOT}/src/io/sog_reader.cpp
    ${GAUSS_FORGE_ROOT}/src/io/sog_writer.cpp
)

# Create static library for internal use
add_library(gauss_forge_py STATIC ${GAUSS_FORGE_SOURCES})
target_include_directories(gauss_forge_py PUBLIC
    ${GAUSS_FORGE_ROOT}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
)
target_link_libraries(gauss_forge_py PUBLIC
    spz::spz
    webp
    nlohmann_json::nlohmann_json
    ZLIB::ZLIB
)

# Create Python extension module
nanobind_add_module(
    _core
    STABLE_ABI
    LTO
    NB_STATIC
    src/gaussforge/bindings.cpp
)
target_link_libraries(_core PRIVATE gauss_forge_py)
target_include_directories(_core PRIVATE
    ${GAUSS_FORGE_ROOT}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

# Install the compiled extension module to the gaussforge package directory
install(TARGETS _core DESTINATION gaussforge)
