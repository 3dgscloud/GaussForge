cmake_minimum_required(VERSION 3.26)
project(gauss_forge
    VERSION 0.2.1
    DESCRIPTION "A high-performance Gaussian Splatting format conversion library"
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate version header from template
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/gf/core/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/gf/core/version.h
    @ONLY
)

include(FetchContent)

# --------------------------------------------------------
# 1. Include WASM configuration (must be before dependencies to manage ZLIB)
# --------------------------------------------------------
if(EMSCRIPTEN)
    include(cmakes/wasm.cmake)
    # Prevent zlib from building shared library which conflicts with static one in WASM
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
endif()

# --------------------------------------------------------
# 2. Include third-party dependencies (SPZ, etc.)
# --------------------------------------------------------
include(cmakes/sog.cmake)
include(cmakes/spz.cmake)

# --------------------------------------------------------
# 3. Compilation options optimization (common and Release)
# --------------------------------------------------------
if(MSVC)
    add_compile_options(/W3)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_definitions(-DDEBUG_BUILD)
    else()
        add_definitions(-DRELEASE_BUILD)
        # MSVC Release optimizations
        add_compile_options(/O2 /GL)
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF /LTCG")
        set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} /LTCG")
    endif()
    
    # Enable sccache if available
    find_program(SCCACHE_PROGRAM sccache)
    if(SCCACHE_PROGRAM)
        set(CMAKE_C_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}")
    endif()
else()
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_definitions(-DDEBUG_BUILD)
    else()
        add_definitions(-DRELEASE_BUILD)
        # GCC/Clang Release optimizations
        add_compile_options(-O3 -flto -funroll-loops -fomit-frame-pointer -ftree-vectorize)
        
        if(NOT EMSCRIPTEN)
            add_compile_options(-march=native)
        endif()
    endif()
endif()

# --------------------------------------------------------
# 4. Define library source files
# --------------------------------------------------------
set(GAUSS_FORGE_SOURCES
    src/core/validate.cpp
    src/io/registry.cpp
    src/io/ply_auto.cpp
    src/io/ply_compressed_reader.cpp
    src/io/ply_compressed_writer.cpp
    src/io/ply_reader.cpp
    src/io/ply_writer.cpp
    src/io/splat_reader.cpp
    src/io/splat_writer.cpp
    src/io/ksplat_reader.cpp
    src/io/ksplat_writer.cpp
    src/io/spz_reader.cpp
    src/io/spz_writer.cpp
    src/io/sog_reader.cpp
    src/io/sog_writer.cpp
)

# Core static library
add_library(gauss_forge STATIC ${GAUSS_FORGE_SOURCES})
target_include_directories(gauss_forge PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
)
# Ensure all targets can link to dependencies
target_link_libraries(gauss_forge PUBLIC spz::spz webp nlohmann_json::nlohmann_json ZLIB::ZLIB)


# --------------------------------------------------------
# 5. Build targets (WASM or CLI)
# --------------------------------------------------------
if(EMSCRIPTEN)
    # WASM export
    add_executable(gauss_forge_wasm src/wasm/bindings.cpp)
    target_link_libraries(gauss_forge_wasm PRIVATE gauss_forge)
    
    target_link_options(gauss_forge_wasm PRIVATE ${GAUSS_FORGE_WASM_LINK_OPTIONS})

    set_target_properties(gauss_forge_wasm PROPERTIES
        OUTPUT_NAME "gauss_forge"
        SUFFIX ".js"
    )
else()
    # Local command-line tool
    add_executable(gfconvert src/cli/main.cpp)
    target_link_libraries(gfconvert PRIVATE gauss_forge)
    add_subdirectory(tests)
endif()