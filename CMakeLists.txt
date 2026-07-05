cmake_minimum_required(VERSION 3.20)

project(figma LANGUAGES C CXX)

option(FIGMA_VIEWER "Build native viewer" ON)
option(FIGMA_TOOLS "Build developer tools" ON)

if(DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY AND NOT IS_ABSOLUTE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(GRAPHICS_EXTERNAL_BUILD ON CACHE BOOL "" FORCE)
set(GRAPHICS_EXAMPLES_BUILD OFF CACHE BOOL "" FORCE)
set(GRAPHICS_TESTS OFF CACHE BOOL "" FORCE)
set(GRAPHICS_TESTS_IN_SOLUTIONS OFF CACHE BOOL "" FORCE)
set(JSON_BINARY OFF CACHE BOOL "" FORCE)
set(JSON_TEST OFF CACHE BOOL "" FORCE)
set(JSON_INSTALL OFF CACHE BOOL "" FORCE)

function(figma_require_thirdparty NAME)
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/thirdparty/${NAME}/CMakeLists.txt")
    message(FATAL_ERROR "Missing thirdparty/${NAME}. Run: sh build/downloads/downloads.sh")
  endif()
endfunction()

figma_require_thirdparty(graphics)
figma_require_thirdparty(json)
figma_require_thirdparty(kiwi)

add_subdirectory(thirdparty/graphics)
add_subdirectory(thirdparty/json)
add_subdirectory(thirdparty/kiwi)

find_package(ZLIB REQUIRED)
find_path(ZSTD_INCLUDE_DIR zstd.h HINTS /opt/homebrew/include /usr/local/include)
find_library(ZSTD_LIBRARY zstd HINTS /opt/homebrew/lib /usr/local/lib)
if(NOT ZSTD_INCLUDE_DIR)
  message(FATAL_ERROR "Missing zstd.h. Install libzstd or pass ZSTD_INCLUDE_DIR.")
endif()
if(NOT ZSTD_LIBRARY)
  message(FATAL_ERROR "Missing libzstd. Install libzstd or pass ZSTD_LIBRARY.")
endif()

if(FIGMA_VIEWER AND APPLE)
  figma_require_thirdparty(libpng)
  figma_require_thirdparty(libjpeg)
  figma_require_thirdparty(freetype)

  set(PNG_BUILD_ZLIB OFF CACHE BOOL "" FORCE)
  set(PNG_SHARED OFF CACHE BOOL "" FORCE)
  set(PNG_STATIC ON CACHE BOOL "" FORCE)
  set(PNG_TESTS OFF CACHE BOOL "" FORCE)
  set(PNG_TOOLS OFF CACHE BOOL "" FORCE)
  unset(PNG_EXECUTABLES CACHE)
  set(PNG_FRAMEWORK OFF CACHE BOOL "" FORCE)
  set(PNG_DEBUG OFF CACHE BOOL "" FORCE)
  set(PNGARG OFF CACHE BOOL "" FORCE)
  add_subdirectory(thirdparty/libpng)

  include(ExternalProject)
  set(FIGMA_LIBJPEG_BUILD_DIR "${CMAKE_BINARY_DIR}/thirdparty/libjpeg")
  get_property(FIGMA_GENERATOR_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  if(FIGMA_GENERATOR_MULTI_CONFIG)
    set(FIGMA_LIBJPEG_LIBRARY "${FIGMA_LIBJPEG_BUILD_DIR}/${CMAKE_BUILD_TYPE}/libjpeg.a")
  else()
    set(FIGMA_LIBJPEG_LIBRARY "${FIGMA_LIBJPEG_BUILD_DIR}/libjpeg.a")
  endif()
  ExternalProject_Add(figma_libjpeg_project
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/libjpeg"
    BINARY_DIR "${FIGMA_LIBJPEG_BUILD_DIR}"
    CMAKE_ARGS
      "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
      "-DCMAKE_CONFIGURATION_TYPES=${CMAKE_BUILD_TYPE}"
      "-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}"
      "-DCMAKE_POSITION_INDEPENDENT_CODE=ON"
      "-DENABLE_SHARED=OFF"
      "-DENABLE_STATIC=ON"
      "-DWITH_TURBOJPEG=OFF"
      "-DWITH_TOOLS=OFF"
      "-DWITH_TESTS=OFF"
      "-DWITH_SIMD=OFF"
      "-DREQUIRE_SIMD=OFF"
    BUILD_BYPRODUCTS "${FIGMA_LIBJPEG_LIBRARY}"
    INSTALL_COMMAND ""
  )
  add_library(figma_libjpeg STATIC IMPORTED GLOBAL)
  set_target_properties(figma_libjpeg PROPERTIES IMPORTED_LOCATION "${FIGMA_LIBJPEG_LIBRARY}")
  add_dependencies(figma_libjpeg figma_libjpeg_project)

  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_HVF ON CACHE BOOL "" FORCE)
  set(FT_ENABLE_ERROR_STRINGS ON CACHE BOOL "" FORCE)
  add_subdirectory(thirdparty/freetype)
endif()

add_subdirectory(sdk)

if(FIGMA_VIEWER)
  add_subdirectory(tools/figma_viewer)
endif()

if(FIGMA_TOOLS)
  add_subdirectory(tools/figma_dump)
endif()
