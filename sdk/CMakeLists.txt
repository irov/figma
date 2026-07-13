add_library(figma_sdk ${FIGMA_SDK_LIBRARY_TYPE}
  src/CanvasPaint.cpp
  src/CanvasPaint.h
  src/Diagnostics.cpp
  src/Diagnostics.h
  src/DiagnosticsMacros.h
  src/DocumentInspection.cpp
  src/DocumentInspection.h
  src/DocumentModel.cpp
  src/DocumentModel.h
  src/Document.h
  src/CanvasDecoder.cpp
  src/CanvasDecoder.h
  src/CanvasDocumentDecoder.cpp
  src/CanvasDocumentDecoder.h
  src/CanvasReader.cpp
  src/CanvasReader.h
  src/CanvasSchema.cpp
  src/CanvasSchema.h
  src/DocumentLoader.h
  src/Document.cpp
  src/DocumentLoader.cpp
  src/JsonDocument.cpp
  src/JsonDocument.h
  src/JsonUtils.cpp
  src/JsonUtils.h
  src/KiwiByteReader.cpp
  src/KiwiByteReader.h
  src/Memory.cpp
  src/Memory.h
  src/Player.cpp
  src/Player.h
  src/RenderList.h
  src/RenderList.cpp
  src/Runtime.cpp
  src/Runtime.h
  src/RuntimeAllocatorMemoryResource.cpp
  src/RuntimeAllocatorMemoryResource.h
  src/ZipArchive.cpp
  src/ZipArchive.h
)

add_library(Figma::SDK ALIAS figma_sdk)

target_compile_features(figma_sdk PUBLIC cxx_std_17)
target_include_directories(figma_sdk PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

if(FIGMA_SDK_DEPENDENCY_INCLUDE_DIRS)
  target_include_directories(figma_sdk PRIVATE ${FIGMA_SDK_DEPENDENCY_INCLUDE_DIRS})
endif()

target_link_libraries(figma_sdk PRIVATE ${FIGMA_SDK_DEPENDENCIES})
target_compile_definitions(figma_sdk PRIVATE
  FIGMA_BUILDING_SDK=1
  ${FIGMA_SDK_COMPILE_DEFINITIONS}
)

get_target_property(FIGMA_SDK_TARGET_TYPE figma_sdk TYPE)

if(FIGMA_SDK_TARGET_TYPE STREQUAL "STATIC_LIBRARY")
  target_compile_definitions(figma_sdk PUBLIC FIGMA_STATIC=1)
endif()

set_target_properties(figma_sdk PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

if(FIGMA_SDK_WARNINGS)
  if(MSVC)
    target_compile_options(figma_sdk PRIVATE /W4)
  else()
    target_compile_options(figma_sdk PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()
