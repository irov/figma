add_executable(figma_sdk_tests
  figma_sdk_tests.c
)
target_compile_features(figma_sdk_tests PRIVATE c_std_11)
set_target_properties(figma_sdk_tests PROPERTIES
  C_STANDARD_REQUIRED YES
  C_EXTENSIONS OFF
)
target_include_directories(figma_sdk_tests PRIVATE
  "${FIGMA_ROOT_DIR}/sdk/src"
  ${FIGMA_SDK_DEPENDENCY_INCLUDE_DIRS}
)
target_link_libraries(figma_sdk_tests PRIVATE ${FIGMA_SDK_TEST_TARGET})
target_compile_options(
  figma_sdk_tests PRIVATE ${FIGMA_SANITIZER_COMPILE_OPTIONS})
target_link_options(
  figma_sdk_tests PRIVATE ${FIGMA_SANITIZER_LINK_OPTIONS})

add_library(figma_c11_header_probe OBJECT
  figma_c11_header_probe.c
)
target_compile_features(figma_c11_header_probe PRIVATE c_std_11)
set_target_properties(figma_c11_header_probe PROPERTIES
  C_STANDARD_REQUIRED YES
  C_EXTENSIONS OFF
)
target_link_libraries(figma_c11_header_probe PRIVATE Figma::SDK)

add_library(figma_cxx17_header_probe OBJECT
  figma_cxx17_header_probe.cpp
)
target_compile_features(figma_cxx17_header_probe PRIVATE cxx_std_17)
target_link_libraries(figma_cxx17_header_probe PRIVATE Figma::SDK)

if(APPLE)
  enable_language(OBJCXX)
  add_library(figma_objcxx17_header_probe OBJECT
    figma_objcxx17_header_probe.mm
  )
  set_target_properties(figma_objcxx17_header_probe PROPERTIES
    OBJCXX_STANDARD 17
    OBJCXX_STANDARD_REQUIRED YES
    OBJCXX_EXTENSIONS OFF
  )
  target_link_libraries(figma_objcxx17_header_probe PRIVATE Figma::SDK)
endif()

if(MSVC)
  target_compile_options(figma_sdk_tests PRIVATE /W4)
else()
  target_compile_options(figma_sdk_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME figma_sdk_tests COMMAND figma_sdk_tests)
