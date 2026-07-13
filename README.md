# figma

C++17 SDK and native viewer for playing local Figma Desktop `.fig` exports.

## Build

After cloning:

```bash
sh build/downloads/downloads.sh
```

```bash
sh build/xcode_macos/make_solution_xcode_macos.sh Debug
sh build/xcode_macos/build_solution_xcode_macos.sh Debug figma_sdk
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

`FIGMA_SDK_DEPENDENCY_INCLUDE_DIRS` is only needed for dependency targets that do not export their include directories. A host may also set `FIGMA_SDK_COMPILE_DEFINITIONS` and disable `FIGMA_SDK_WARNINGS`. Standalone builds keep bundled dependencies, the viewer, and tools enabled by default; those defaults are disabled when Figma is used as a subdirectory.

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
