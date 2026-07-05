add_executable(figma_dump
  Main.cpp
)

target_compile_features(figma_dump PRIVATE cxx_std_17)
target_link_libraries(figma_dump PRIVATE figma_sdk)
target_include_directories(figma_dump PRIVATE "${CMAKE_SOURCE_DIR}/sdk/src")

if(MSVC)
  target_compile_options(figma_dump PRIVATE /W4)
else()
  target_compile_options(figma_dump PRIVATE -Wall -Wextra -Wpedantic)
endif()
