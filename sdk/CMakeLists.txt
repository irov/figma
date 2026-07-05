add_library(figma_sdk STATIC
  src/CanvasPaint.cpp
  src/CanvasPaint.h
  src/Diagnostics.cpp
  src/Diagnostics.h
  src/DocumentInspection.h
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
  src/Player.cpp
  src/PlayerFactory.h
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

target_compile_features(figma_sdk PUBLIC cxx_std_17)
target_include_directories(figma_sdk PUBLIC include)
target_include_directories(figma_sdk PRIVATE
  ../thirdparty/graphics/include
  ../thirdparty/json/include
  ../thirdparty/kiwi/include
)
target_link_libraries(figma_sdk PRIVATE graphics json kiwi ZLIB::ZLIB)
target_compile_definitions(figma_sdk PRIVATE FIGMA_BUILDING_SDK=1)
target_include_directories(figma_sdk PRIVATE ${ZSTD_INCLUDE_DIR})
target_link_libraries(figma_sdk PRIVATE ${ZSTD_LIBRARY})
get_target_property(FIGMA_SDK_TARGET_TYPE figma_sdk TYPE)
if(FIGMA_SDK_TARGET_TYPE STREQUAL "STATIC_LIBRARY")
  target_compile_definitions(figma_sdk PUBLIC FIGMA_STATIC=1)
endif()
set_target_properties(figma_sdk PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

if(MSVC)
  target_compile_options(figma_sdk PRIVATE /W4)
else()
  target_compile_options(figma_sdk PRIVATE -Wall -Wextra -Wpedantic)
endif()
