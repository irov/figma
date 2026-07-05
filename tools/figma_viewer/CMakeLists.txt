if(APPLE)
  enable_language(OBJCXX)

  add_executable(figma_viewer MACOSX_BUNDLE
    FigmaAppDelegate.h
    FigmaAppDelegate.mm
    FigmaViewerShared.h
    FigmaViewerShared.mm
    FigmaViewerView.h
    FigmaViewerView.mm
    FreeTypeTextRenderer.h
    FreeTypeTextRenderer.mm
    Main.mm
    MetalRenderBackend.h
    MetalRenderBackend.mm
  )

  target_link_libraries(figma_viewer PRIVATE figma_sdk freetype png_static figma_libjpeg)
  target_include_directories(figma_viewer PRIVATE
    "${CMAKE_SOURCE_DIR}/sdk/src"
    "${CMAKE_SOURCE_DIR}/thirdparty/libjpeg/src"
    "${FIGMA_LIBJPEG_BUILD_DIR}"
  )
  target_compile_features(figma_viewer PRIVATE cxx_std_17)
  target_compile_options(figma_viewer PRIVATE -fobjc-arc)
  set_target_properties(figma_viewer PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "Figma Viewer"
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.figma.viewer"
  )

  find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
  find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
  find_library(METAL_FRAMEWORK Metal REQUIRED)
  find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
  target_link_libraries(figma_viewer PRIVATE
    ${APPKIT_FRAMEWORK}
    ${FOUNDATION_FRAMEWORK}
    ${METAL_FRAMEWORK}
    ${QUARTZCORE_FRAMEWORK}
  )
else()
  message(STATUS "figma_viewer is currently implemented for macOS/AppKit only")
endif()
