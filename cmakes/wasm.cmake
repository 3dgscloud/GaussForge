# cmakes/wasm.cmake

if(NOT EMSCRIPTEN)
    return()
endif()

message(STATUS "Configuring Emscripten specific settings...")

# 1. Core compiler flags
# Enable SIMD 128, disable finite math-only optimization to support Inf/NaN in 3DGS
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msimd128 -fno-finite-math-only")

# 2. ZLIB dummy logic: resolve FindZLIB errors
set(DUMMY_ZLIB "${CMAKE_CURRENT_BINARY_DIR}/libz_placeholder.a")
if(NOT EXISTS "${DUMMY_ZLIB}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/dummy.c" "")
    execute_process(COMMAND ${CMAKE_C_COMPILER} -c "${CMAKE_CURRENT_BINARY_DIR}/dummy.c" -o "${CMAKE_CURRENT_BINARY_DIR}/dummy.o")
    execute_process(COMMAND ${CMAKE_AR} rcs "${DUMMY_ZLIB}" "${CMAKE_CURRENT_BINARY_DIR}/dummy.o")
endif()

set(ZLIB_FOUND TRUE CACHE BOOL "" FORCE)
set(ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "" FORCE)
set(ZLIB_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "" FORCE)
set(ZLIB_LIBRARY "${DUMMY_ZLIB}" CACHE FILEPATH "" FORCE)
set(ZLIB_LIBRARIES "${DUMMY_ZLIB}" CACHE FILEPATH "" FORCE)

if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB INTERFACE IMPORTED)
    set_target_properties(ZLIB::ZLIB PROPERTIES INTERFACE_LINK_LIBRARIES "${DUMMY_ZLIB}")
endif()

# 3. Define WASM link options list
# Use SHELL: syntax to ensure arguments are passed as-is to emcc
set(GAUSS_FORGE_WASM_LINK_OPTIONS
    "SHELL:-O3"
    "SHELL:-flto"
    "SHELL:--bind"
    "SHELL:-msimd128"
    "-sWASM=1"
    "-sMODULARIZE=1"
    "-sEXPORT_NAME='createGaussForgeModule'"
    "-sEXPORT_ES6=1"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINITIAL_MEMORY=268435456"
    "-sMAXIMUM_MEMORY=2GB"
    "-sUSE_ZLIB=1"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','stringToUTF8']"
    "-sERROR_ON_UNDEFINED_SYMBOLS=0"
    "-sASSERTIONS=0"
)

add_definitions(-DNO_FILESYSTEM)