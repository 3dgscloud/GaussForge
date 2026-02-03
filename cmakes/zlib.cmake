# ZLIB - compression library (shared by SOG, SPZ, and other formats)

FetchContent_Declare(
  zlib
  GIT_REPOSITORY https://github.com/madler/zlib.git
  GIT_TAG v1.3.1
  GIT_SHALLOW TRUE
)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# Patch zlib to not create shared library target in WASM
if(EMSCRIPTEN)
  set(CMAKE_SHARED_LIBRARY_PREFIX "_")
  set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
endif()

FetchContent_MakeAvailable(zlib)

if(EMSCRIPTEN)
  # zlib creates both zlib (SHARED) and zlibstatic (STATIC). Both try to output
  # 'libz.a' on Emscripten. Rename zlibstatic to avoid conflict.
  if(TARGET zlibstatic)
    set_target_properties(zlibstatic PROPERTIES OUTPUT_NAME z_static)
  endif()
endif()

# zlib's CMakeLists always creates zlib (SHARED) and zlibstatic (STATIC).
# It does NOT honor BUILD_SHARED_LIBS. We use zlibstatic so gauss_forge and
# spz link statically (no libz.dylib/.so at runtime).
set(ZLIB_FOUND TRUE CACHE BOOL "" FORCE)
set(ZLIB_INCLUDE_DIR "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}" CACHE PATH "" FORCE)
set(ZLIB_LIBRARY zlibstatic CACHE FILEPATH "" FORCE)
set(ZLIB_LIBRARIES zlibstatic CACHE FILEPATH "" FORCE)

if(NOT TARGET ZLIB::ZLIB)
  add_library(ZLIB::ZLIB INTERFACE IMPORTED)
  target_link_libraries(ZLIB::ZLIB INTERFACE zlibstatic)
  target_include_directories(ZLIB::ZLIB INTERFACE "${zlib_SOURCE_DIR}" "${zlib_BINARY_DIR}")
endif()
