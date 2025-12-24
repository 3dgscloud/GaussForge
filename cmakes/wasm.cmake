# cmakes/wasm.cmake

if(NOT EMSCRIPTEN)
    return()
endif()

message(STATUS "Configuring Emscripten specific settings...")

# 1. Core compiler flags
# Enable SIMD 128, disable finite math-only optimization to support Inf/NaN in 3DGS
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msimd128 -fno-finite-math-only")

# 2. ZLIB handling: Let FetchContent manage it completely

# 3. Define WASM link options list
# Use SHELL: syntax to ensure arguments are passed as-is to emcc
# WASM_ENVIRONMENT can be set via CMake variable (default: web,worker)
if(NOT DEFINED WASM_ENVIRONMENT)
    set(WASM_ENVIRONMENT "web,worker")
endif()

set(GAUSS_FORGE_WASM_LINK_OPTIONS
    "SHELL:-O3"
    "SHELL:-flto"
    "SHELL:--bind"
    "SHELL:-msimd128"
    "-sWASM=1"
    "-sMODULARIZE=1"
    "-sSINGLE_FILE=1"
    "-sEXPORT_NAME='createGaussForgeModule'"
    "-sEXPORT_ES6=1"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINITIAL_MEMORY=268435456"
    "-sMAXIMUM_MEMORY=2GB"
    "-sUSE_ZLIB=0"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','stringToUTF8']"
    "-sERROR_ON_UNDEFINED_SYMBOLS=0"
    "-sASSERTIONS=0"
    "-sENVIRONMENT=${WASM_ENVIRONMENT}"
)

add_definitions(-DNO_FILESYSTEM)