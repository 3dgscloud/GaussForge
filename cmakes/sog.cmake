# SOG format dependencies: libwebp, nlohmann_json, and zlib

# ZLIB for ZIP creation and SPZ
FetchContent_Declare(
  zlib
  GIT_REPOSITORY https://github.com/madler/zlib.git
  GIT_TAG v1.3.1
  GIT_SHALLOW TRUE
)
# ZLIB build options
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# Patch zlib to not create shared library target in WASM
if(EMSCRIPTEN)
  # Override the shared library target name to avoid conflict
  set(CMAKE_SHARED_LIBRARY_PREFIX "_")
  set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
endif()

FetchContent_MakeAvailable(zlib)

if(EMSCRIPTEN)
  # When BUILD_SHARED_LIBS is OFF (set in wasm.cmake), zlib's CMakeLists.txt
  # creates two static libraries: 'zlib' and 'zlibstatic'.
  # Both try to output 'libz.a'. Ninja detects this as a conflict even if
  # one target is EXCLUDE_FROM_ALL.
  # We resolve this by renaming zlibstatic's output name.
  if(TARGET zlibstatic)
    set_target_properties(zlibstatic PROPERTIES OUTPUT_NAME z_static)
  endif()
endif()

# Set variables for other packages that use find_package(ZLIB)
set(ZLIB_FOUND TRUE CACHE BOOL "" FORCE)
set(ZLIB_INCLUDE_DIR "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}" CACHE PATH "" FORCE)
# Point to the main 'zlib' target (which is static in WASM build) to align with dependencies
set(ZLIB_LIBRARY zlib CACHE FILEPATH "" FORCE)
set(ZLIB_LIBRARIES zlib CACHE FILEPATH "" FORCE)

if(NOT TARGET ZLIB::ZLIB)
  add_library(ZLIB::ZLIB INTERFACE IMPORTED)
  target_link_libraries(ZLIB::ZLIB INTERFACE zlib)
  target_include_directories(ZLIB::ZLIB INTERFACE "${zlib_SOURCE_DIR}" "${zlib_BINARY_DIR}")
endif()

# libwebp for WebP image decoding
FetchContent_Declare(
  libwebp
  GIT_REPOSITORY https://github.com/webmproject/libwebp.git
  GIT_TAG v1.4.0
  GIT_SHALLOW TRUE
)

# Disable libwebp extras we don't need
set(WEBP_BUILD_ANIM_UTILS OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_CWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_DWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_GIF2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_IMG2WEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_VWEBP OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPINFO OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_LIBWEBPMUX OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_WEBPMUX OFF CACHE BOOL "" FORCE)
set(WEBP_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(libwebp)

# nlohmann_json for meta.json parsing
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
  GIT_SHALLOW TRUE
)

set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(nlohmann_json)
