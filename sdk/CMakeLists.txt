set(FIGMA_SDK_SOURCES
  src/figma_canvas.c
  src/figma_canvas.h
  src/figma_canvas_decoder.c
  src/figma_core.c
  src/figma_document_loader.c
  src/figma_inspection.c
  src/figma_inspection.h
  src/figma_internal.h
  src/figma_json.c
  src/figma_json.h
  src/figma_model.c
  src/figma_model.h
  src/figma_number.c
  src/figma_player.c
  src/figma_player.h
  src/figma_zip.c
  src/figma_zip.h
)

add_library(figma_sdk ${FIGMA_SDK_LIBRARY_TYPE}
  ${FIGMA_SDK_SOURCES}
)
add_library(Figma::SDK ALIAS figma_sdk)

target_compile_features(figma_sdk PUBLIC c_std_11)
set_target_properties(figma_sdk PROPERTIES
  C_STANDARD_REQUIRED YES
  C_EXTENSIONS NO
  C_VISIBILITY_PRESET hidden
  POSITION_INDEPENDENT_CODE YES
)
target_include_directories(figma_sdk PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_include_directories(figma_sdk PRIVATE
  ${FIGMA_SDK_DEPENDENCY_INCLUDE_DIRS}
)
target_compile_definitions(figma_sdk PRIVATE
  FIGMA_BUILDING_SDK=1
  ${FIGMA_SDK_COMPILE_DEFINITIONS}
)
target_link_libraries(figma_sdk PRIVATE ${FIGMA_SDK_DEPENDENCIES})
target_compile_options(
  figma_sdk PRIVATE ${FIGMA_SANITIZER_COMPILE_OPTIONS})
target_link_options(figma_sdk PRIVATE ${FIGMA_SANITIZER_LINK_OPTIONS})

if(FIGMA_SDK_WARNINGS)
  if(MSVC)
    target_compile_options(figma_sdk PRIVATE /W4)
  else()
    target_compile_options(figma_sdk PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()

get_target_property(FIGMA_SDK_TARGET_TYPE figma_sdk TYPE)
if(FIGMA_SDK_TARGET_TYPE STREQUAL "SHARED_LIBRARY")
  if(APPLE)
    set(FIGMA_SDK_EXPORTS_FILE
      "${CMAKE_CURRENT_BINARY_DIR}/figma_sdk.exports")
    file(WRITE "${FIGMA_SDK_EXPORTS_FILE}" "_figma_*\n")
    target_link_options(figma_sdk PRIVATE
      "LINKER:-exported_symbols_list,${FIGMA_SDK_EXPORTS_FILE}"
    )
  elseif(UNIX)
    target_link_options(figma_sdk PRIVATE "LINKER:--exclude-libs,ALL")
  endif()
endif()

if(FIGMA_SDK_TARGET_TYPE STREQUAL "STATIC_LIBRARY")
  target_compile_definitions(figma_sdk PUBLIC FIGMA_STATIC=1)
  set(FIGMA_SDK_TEST_TARGET Figma::SDK PARENT_SCOPE)
elseif(FIGMA_TESTS)
  add_library(figma_sdk_test_static STATIC
    ${FIGMA_SDK_SOURCES}
  )
  target_compile_features(figma_sdk_test_static PUBLIC c_std_11)
  set_target_properties(figma_sdk_test_static PROPERTIES
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
    C_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE YES
  )
  target_compile_definitions(figma_sdk_test_static PUBLIC FIGMA_STATIC=1)
  target_compile_definitions(figma_sdk_test_static PRIVATE
    FIGMA_BUILDING_SDK=1
    ${FIGMA_SDK_COMPILE_DEFINITIONS}
  )
  target_include_directories(figma_sdk_test_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  )
  target_include_directories(figma_sdk_test_static PRIVATE
    ${FIGMA_SDK_DEPENDENCY_INCLUDE_DIRS}
  )
  target_link_libraries(
    figma_sdk_test_static PRIVATE ${FIGMA_SDK_DEPENDENCIES})
  target_compile_options(
    figma_sdk_test_static PRIVATE ${FIGMA_SANITIZER_COMPILE_OPTIONS})
  if(FIGMA_SDK_WARNINGS)
    if(MSVC)
      target_compile_options(figma_sdk_test_static PRIVATE /W4)
    else()
      target_compile_options(
        figma_sdk_test_static PRIVATE -Wall -Wextra -Wpedantic)
    endif()
  endif()
  set(FIGMA_SDK_TEST_TARGET figma_sdk_test_static PARENT_SCOPE)
else()
  set(FIGMA_SDK_TEST_TARGET Figma::SDK PARENT_SCOPE)
endif()
