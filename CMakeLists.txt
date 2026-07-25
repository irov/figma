cmake_minimum_required(VERSION 3.20)

project(figma LANGUAGES C)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(FIGMA_STANDALONE ON)
else()
  set(FIGMA_STANDALONE OFF)
endif()

option(FIGMA_BUILD_BUNDLED_DEPENDENCIES "Build bundled SDK dependencies" ${FIGMA_STANDALONE})
option(FIGMA_VIEWER "Build native viewer" ${FIGMA_STANDALONE})
option(FIGMA_TOOLS "Build developer tools" ${FIGMA_STANDALONE})
option(FIGMA_TESTS "Build SDK tests" ${FIGMA_STANDALONE})
option(FIGMA_SDK_WARNINGS "Enable SDK compiler warnings" ON)
option(FIGMA_SANITIZERS "Enable ASan and UBSan for first-party targets" OFF)

if(FIGMA_SANITIZERS)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    set(FIGMA_SANITIZER_COMPILE_OPTIONS
      -fsanitize=address,undefined
      -fno-omit-frame-pointer
    )
    set(FIGMA_SANITIZER_LINK_OPTIONS
      -fsanitize=address,undefined
    )
  else()
    message(FATAL_ERROR
      "FIGMA_SANITIZERS requires a Clang- or GCC-compatible toolchain"
    )
  endif()
endif()

if(NOT DEFINED FIGMA_SDK_LIBRARY_TYPE)
  set(FIGMA_SDK_LIBRARY_TYPE STATIC)
endif()

string(TOUPPER "${FIGMA_SDK_LIBRARY_TYPE}" FIGMA_SDK_LIBRARY_TYPE)

if(NOT FIGMA_SDK_LIBRARY_TYPE STREQUAL "STATIC" AND NOT FIGMA_SDK_LIBRARY_TYPE STREQUAL "SHARED")
  message(FATAL_ERROR "FIGMA_SDK_LIBRARY_TYPE must be STATIC or SHARED")
endif()

if(NOT DEFINED FIGMA_SDK_DEPENDENCIES)
  set(FIGMA_SDK_DEPENDENCIES)
endif()

if(NOT DEFINED FIGMA_SDK_DEPENDENCY_INCLUDE_DIRS)
  set(FIGMA_SDK_DEPENDENCY_INCLUDE_DIRS)
endif()

if(NOT DEFINED FIGMA_SDK_COMPILE_DEFINITIONS)
  set(FIGMA_SDK_COMPILE_DEFINITIONS)
endif()

if(FIGMA_STANDALONE AND DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY AND NOT IS_ABSOLUTE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()

if(FIGMA_STANDALONE)
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

set(FIGMA_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

function(figma_require_thirdparty _name)
  if(NOT EXISTS "${FIGMA_ROOT_DIR}/thirdparty/${_name}/CMakeLists.txt")
    message(FATAL_ERROR "Missing thirdparty/${_name}. Run: sh build/downloads/downloads.sh")
  endif()
endfunction()

if(FIGMA_BUILD_BUNDLED_DEPENDENCIES)
  set(GRAPHICS_EXTERNAL_BUILD ON CACHE BOOL "" FORCE)
  set(GRAPHICS_EXAMPLES_BUILD OFF CACHE BOOL "" FORCE)
  set(GRAPHICS_TESTS OFF CACHE BOOL "" FORCE)
  set(GRAPHICS_TESTS_IN_SOLUTIONS OFF CACHE BOOL "" FORCE)
  set(JSON_BINARY OFF CACHE BOOL "" FORCE)
  set(JSON_TEST OFF CACHE BOOL "" FORCE)
  set(JSON_INSTALL OFF CACHE BOOL "" FORCE)

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

  add_library(figma_zstd UNKNOWN IMPORTED)
  set_target_properties(figma_zstd PROPERTIES
    IMPORTED_LOCATION "${ZSTD_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${ZSTD_INCLUDE_DIR}"
  )

  set(FIGMA_SDK_DEPENDENCIES
    graphics
    json
    kiwi
    ZLIB::ZLIB
    figma_zstd
  )
  set(FIGMA_SDK_DEPENDENCY_INCLUDE_DIRS
    "${FIGMA_ROOT_DIR}/thirdparty/graphics/include"
    "${FIGMA_ROOT_DIR}/thirdparty/json/include"
  )
elseif(NOT FIGMA_SDK_DEPENDENCIES)
  message(FATAL_ERROR
    "FIGMA_BUILD_BUNDLED_DEPENDENCIES is OFF, but FIGMA_SDK_DEPENDENCIES is empty. "
    "Pass the host dependency targets before add_subdirectory(figma)."
  )
endif()

foreach(FIGMA_SDK_DEPENDENCY IN LISTS FIGMA_SDK_DEPENDENCIES)
  if(NOT TARGET "${FIGMA_SDK_DEPENDENCY}")
    message(FATAL_ERROR "Figma SDK dependency target does not exist: ${FIGMA_SDK_DEPENDENCY}")
  endif()
endforeach()

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
  set(FIGMA_LIBJPEG_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/thirdparty/libjpeg")
  get_property(FIGMA_GENERATOR_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

  if(FIGMA_GENERATOR_MULTI_CONFIG)
    set(FIGMA_LIBJPEG_LIBRARY "${FIGMA_LIBJPEG_BUILD_DIR}/${CMAKE_BUILD_TYPE}/libjpeg.a")
  else()
    set(FIGMA_LIBJPEG_LIBRARY "${FIGMA_LIBJPEG_BUILD_DIR}/libjpeg.a")
  endif()

  ExternalProject_Add(figma_libjpeg_project
    SOURCE_DIR "${FIGMA_ROOT_DIR}/thirdparty/libjpeg"
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

if(FIGMA_TESTS)
  enable_language(CXX)
  enable_testing()
  add_subdirectory(tests)
endif()

if(FIGMA_VIEWER)
  add_subdirectory(tools/figma_viewer)
endif()

if(FIGMA_TOOLS)
  add_subdirectory(tools/figma_dump)
endif()
