add_executable(figma_dump
  Main.c
)

target_compile_features(figma_dump PRIVATE c_std_11)
set_target_properties(figma_dump PROPERTIES
  C_STANDARD_REQUIRED YES
  C_EXTENSIONS OFF
)
target_include_directories(figma_dump PRIVATE
  "${FIGMA_ROOT_DIR}/sdk/src"
)
target_link_libraries(figma_dump PRIVATE Figma::SDK)
target_compile_options(
  figma_dump PRIVATE ${FIGMA_SANITIZER_COMPILE_OPTIONS})
target_link_options(
  figma_dump PRIVATE ${FIGMA_SANITIZER_LINK_OPTIONS})

if(MSVC)
  target_compile_options(figma_dump PRIVATE /W4)
else()
  target_compile_options(figma_dump PRIVATE -Wall -Wextra -Wpedantic)
endif()
