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
    ViewerRenderTypes.h
  )

  target_link_libraries(figma_viewer PRIVATE Figma::SDK freetype png_static figma_libjpeg)
  target_include_directories(figma_viewer PRIVATE
    "${FIGMA_ROOT_DIR}/sdk/src"
    ${FIGMA_SDK_DEPENDENCY_INCLUDE_DIRS}
    "${FIGMA_ROOT_DIR}/thirdparty/libjpeg/src"
    "${FIGMA_LIBJPEG_BUILD_DIR}"
  )
  target_compile_features(figma_viewer PRIVATE cxx_std_17)
  target_compile_options(figma_viewer PRIVATE -fobjc-arc)
  target_link_options(
    figma_viewer PRIVATE ${FIGMA_SANITIZER_LINK_OPTIONS})
  set_target_properties(figma_viewer PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "Figma Viewer"
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.figma.viewer"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "dev.figma.viewer"
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
