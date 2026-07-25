# figma

C11 SDK, C11 inspection CLI, and native Objective-C++ macOS viewer for playing local Figma Desktop `.fig` exports.

The public ABI is version 7 and lives in:

- `sdk/include/figma/figma.h` — strict C11 API for C clients;
- `sdk/include/figma/figma.hpp` — the only C++/Objective-C++ linkage adapter.

The SDK exposes opaque runtime/document/player/render-list handles, fixed-width descriptors, `int32_t` enum value types with explicit `FIGMA_*` constants, and ordinary `figma_*` functions. It has no STL, exceptions, RTTI, virtual interfaces, or C++ runtime dependency. A runtime must outlive its documents, and a document must outlive its players.

Example:

```c
#include "figma/figma.h"

figma_runtime_desc_t desc = {0};
figma_runtime_t *runtime = NULL;
figma_result_t result =
    figma_runtime_create(FIGMA_SDK_VERSION, &desc, &runtime);

if(result == FIGMA_RESULT_OK)
{
    figma_runtime_destroy(runtime);
}
```

Objective-C++ and C++ code includes `figma/figma.hpp` instead. It is only an `extern "C"` include adapter and intentionally provides no C++ wrapper classes.

## Build

After cloning:

```bash
sh build/downloads/downloads.sh
```

```bash
sh build/xcode_macos/make_solution_xcode_macos.sh Debug
sh build/xcode_macos/build_solution_xcode_macos.sh Debug figma_sdk
sh build/xcode_macos/build_solution_xcode_macos.sh Debug figma_dump
sh build/xcode_macos/build_solution_xcode_macos.sh Debug figma_sdk_tests
ctest --test-dir solutions/solution_xcode_macos -C Debug --output-on-failure
```

On macOS, build the viewer:

```bash
sh build/xcode_macos/build_solution_xcode_macos.sh Debug figma_viewer
open -n "$PWD/solutions/bin/xcode_macos/Debug/figma_viewer.app" --args /Users/yurii.levchenko/Downloads/KROSSROAD_Presentation.fig
```

### Embed with CMake

The repository root can be included directly by a host project. The host creates the dependency targets first and passes them to Figma instead of maintaining a separate SDK source list:

```cmake
set(FIGMA_BUILD_BUNDLED_DEPENDENCIES OFF)
set(FIGMA_VIEWER OFF)
set(FIGMA_TOOLS OFF)
set(FIGMA_TESTS OFF)
set(FIGMA_SDK_LIBRARY_TYPE STATIC)
set(FIGMA_SDK_DEPENDENCIES
  graphics
  json
  kiwi
  zlibstatic
  libzstd_static
)
set(FIGMA_SDK_DEPENDENCY_INCLUDE_DIRS
  "${THIRDPARTY_DIR}/graphics/include"
  "${THIRDPARTY_DIR}/json/include"
)

add_subdirectory("${THIRDPARTY_DIR}/figma" "${CMAKE_BINARY_DIR}/figma")
target_link_libraries(host_target PRIVATE Figma::SDK)
```

`FIGMA_SDK_DEPENDENCY_INCLUDE_DIRS` is only needed for dependency targets that do not export their include directories. A host may also set `FIGMA_SDK_COMPILE_DEFINITIONS` and disable `FIGMA_SDK_WARNINGS`. Standalone builds keep bundled dependencies, the viewer, tools, and `FIGMA_TESTS` enabled by default; those defaults are disabled when Figma is used as a subdirectory.

SDK-owned memory goes through the allocator callbacks in `figma_runtime_desc_t`; zero callbacks select the default C allocator. Operations return `figma_result_t`, and failed parses/OOM do not publish partially initialized handles or state. A failed `figma_document_load_ux` preserves the previous UX mappings.

Callback tables (`figma_action_router_t` and `figma_data_context_t`) are copied by the player, but their `user_data` remains host-owned. Callback string views are synchronous-only. Asset, diagnostic, render-batch, and generated-text buffers are read-only borrowed views; their owner/mutation lifetimes are documented in `figma.h`.

The viewer requires a local `.fig` path argument and starts in prototype viewport mode by default with an iPhone 14 Plus aspect ratio. It draws only the rectangular screen viewport, not a phone body. If the requested `.fig` path is missing but `<path>.zip` exists, the viewer opens the ZIP export automatically.

Text rendering uses decoded Figma font metadata and searches `FIGMA_VIEWER_FONT_DIRS` plus system font directories. Missing `.fig` fonts are logged by the viewer instead of being bundled as sample-specific fallbacks.

## UX `.ux.json`

The SDK uses optional `.ux.json` data for game-facing bindings and actions:

```json
{
  "bindings": [
    { "nodeId": "title", "key": "screen.title", "property": "text" }
  ],
  "actions": [
    { "nodeId": "play_button", "actionId": "play", "targetFrameId": "gameplay", "trigger": "click" }
  ]
}
```

The SDK validates the local `.fig` ZIP container through zlib-backed ZIP reading. `canvas.fig` scene chunks are decoded through the embedded Kiwi schema; raw deflate uses zlib and Zstandard scene data requires `libzstd`. The current renderer covers the first practical subset: image fills with SDK-provided geometry/UV, text metadata with decoded lines, `irov/graphics` generated mesh geometry for solid fills/strokes/rounded rectangles/ellipses/ellipse arcs, simple component instance expansion via `symbolData.symbolID`, and a decoded prototype start frame. Unsupported or missing canvas/prototype/visual data produces diagnostics and skipped commands, not preview rendering.

## Tests

`FIGMA_TESTS` provides the C11 `figma_sdk_tests` CTest target and compile probes for `figma.h` from C11 and `figma.hpp` from C++17/Objective-C++17. The synthetic tests cover version/argument failures, allocator fault injection and cleanup, callbacks, bindings, navigation, pointer capture, animation, render lists, atomic UX replacement, and locale-independent number formatting.

`figma_dump` remains output-compatible with the pre-migration tool for the verification fixture in default, `--render-list`, `--animations`, `--find`, and `--node` modes. It and the viewer use the repository-private C inspection accessors; decoded containers and concrete SDK objects are not part of the public ABI.

Set `FIGMA_SANITIZERS=ON` with Clang or GCC to instrument first-party SDK, dump, and test code with ASan/UBSan without folding vendored dependency findings into that gate.
