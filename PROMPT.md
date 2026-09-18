# Figma SDK Project Prompt

This file is the durable product prompt and source of requirements for this repository. Update it in the same change whenever architecture, public API, `.fig` import, binding schema, viewer UX, build rules, tests, or roadmap change.

## 1. Purpose

`figma` is a native C11 SDK with a C11 inspection CLI and an Objective-C++ macOS viewer for playing local Figma UI/UX exports in host applications.

The SDK parses host-provided Figma Desktop `.fig` export bytes, keeps document and playback state, handles prototype input, data binding, and host callbacks, then returns a backend-free render list. The host engine or viewer owns filesystem/resource IO and the actual rendering.

The first verification fixture is:

```text
/Users/yurii.levchenko/Downloads/KROSSROAD_Presentation.fig
```

That file is a ZIP archive containing `meta.json`, `thumbnail.png`, `images/*`, and binary `canvas.fig` with a `fig-kiwi` prefix.

## 2. Core Decisions

- SDK language is ISO C11 with extensions disabled. SDK sources must not depend on the STL, exceptions, RTTI, virtual dispatch, or a C++ runtime.
- SDK target name is `figma_sdk`.
- The public ABI is C and uses opaque `figma_*_t` handles, fixed-width fields, `int32_t` enum value types with explicit `FIGMA_*` constants, `figma_bool_t` based on `uint8_t`, borrowed string/byte views, and ordinary `figma_*` functions.
- CMake is the canonical build system.
- The repository root CMake project is reusable through `add_subdirectory`. Standalone builds use downloaded bundled dependencies and enable the viewer/tools by default; embedded builds disable those defaults and receive host-created dependency targets, any legacy non-transitive dependency include directories, optional private compile definitions, and the requested `STATIC` or `SHARED` SDK library type through `FIGMA_*` parameters. Hosts link the stable `Figma::SDK` alias and must not maintain a duplicate `figma_sdk` source list.
- Thirdparty source acquisition follows the Mengine-style downloads project: run `sh build/downloads/downloads.sh` to configure/build the downloads solution under `solutions/downloads` and populate `thirdparty/*`. The repository must not rely on Git submodules for normal dependency setup.
- `build/` is a script-entrypoint directory only and is grouped by build family, for example `build/downloads/` and `build/xcode_macos/`; future MSVC or other toolchain entrypoints must get their own subdirectories. Generated CMake/Xcode solutions, binaries, and app bundles live under ignored `solutions/*` paths.
- `.fig` archive bytes are the primary v1 input; no Figma API or network auth is required. SDK code does not open filesystem paths or own other system IO. Host tools, viewers, or engine integrations load resources through their own IO layer and pass `.fig` data as a raw pointer plus byte size and optional `.ux.json` strings into the SDK.
- SDK is backend-agnostic and must not depend on AppKit, Electron, DOM, CSS, OpenGL, Metal, Vulkan, or DirectX.
- `sdk/include/figma/figma.h` is a strict C11 header and contains no `__cplusplus` branch or `extern "C"`. C11 SDK clients, `figma_dump`, and C tests include it directly.
- `sdk/include/figma/figma.hpp` is the only C++ linkage adapter. It contains only an `extern "C"` block that includes `figma/figma.h`; it does not define classes, namespaces, or compatibility wrappers. Objective-C++ and C++ clients include this adapter.
- SDK-owned handles are passed as pointers so optional state and ownership remain explicit. The runtime must outlive its documents, and a document must outlive its players. Each handle is destroyed through its matching `figma_*_destroy` function.
- `figma_action_router_t` and `figma_data_context_t` are copied callback tables with host-owned `user_data`. Callback string views are valid only for the synchronous callback call.
- The public header defines `FIGMA_SDK_VERSION` as `8`; hosts pass it to `figma_runtime_create`. Version mismatch returns `FIGMA_RESULT_VERSION_MISMATCH` before reading the runtime descriptor or invoking allocator hooks, and the output handle remains `NULL`. Increment `FIGMA_SDK_VERSION` for every incompatible public ABI change.
- Asset, diagnostic, render-batch, and generated-text data are returned as read-only borrowed views with owner/mutation lifetimes documented in `figma.h`.
- SDK returns render lists as read-only render batches: geometry vertices/indices plus texture or asset references where applicable; host/viewer uploads resources and renders them. Public clients enumerate them with `figma_render_list_get_batch_count` and `figma_render_list_get_batch`. Figma-specific command metadata, decoded canvas/prototype descriptors, and inspection functions are a private C tooling API under `sdk/src/figma_inspection.h`.
- SDK owns prototype animation timing and composition. Viewer/backend code calls `figma_player_update(player, dt)` and renders the SDK-produced `figma_render_list_t`; it must not implement Figma-specific tweening or navigation animation logic.
- `figma_runtime_desc_t` carries the host-owned `gp_graphics_t` shared by every player of that runtime. It is required: `figma_runtime_create` returns `FIGMA_RESULT_INVALID_ARGUMENT` without it. Canvases and paths are always created from that object, and the SDK never creates or destroys a graphics object of its own.
- Geometry generation uses `irov/graphics`. Viewer/backend code must not construct Figma shape geometry with AppKit/CoreGraphics bezier helpers; if `irov/graphics` lacks a required primitive, extend `irov/graphics` instead of adding backend-only or SDK-local geometry. `irov/graphics` rendering must preserve primitive insertion order across primitive types so Figma z-order is not lost during mesh generation.
- JSON parsing and `.ux.json` reading use `irov/json`.
- Kiwi binary primitive reading uses `irov/kiwi`, a small pure-C thirdparty library under `thirdparty/kiwi`; its primitive reader API returns values directly and uses assertions for reader bounds/varint invariants, while Figma-specific schema interpretation, external file validation, diagnostics, and normalized document construction stay in `figma_sdk`.
- ZIP reading uses system `zlib`; the SDK owns the minimal ZIP container parser needed for host-provided `.fig` archive bytes.
- `canvas.fig` Kiwi chunks are decompressed with raw deflate through `zlib` and required `libzstd` when scene data uses Zstandard.
- The first native viewer is a macOS AppKit shell with a Metal-backed Figma viewport and lives outside the SDK. AppKit owns the window, controls, input routing, and debug/inspector UI; Metal owns final viewport composition.
- Viewer text rasterization uses downloaded FreeType from `thirdparty/freetype` and resolves font files from decoded Figma font metadata through viewer-configured and local system font directories; the viewer and SDK must not commit or bundle sample-specific `.fig` fonts. Missing fonts are reported by the viewer instead of guessed through bundled fallbacks, then generated text bitmaps are uploaded as Metal textures when a matching face is found. PNG image assets are decoded through downloaded `libpng`; JPEG image assets are decoded through downloaded `libjpeg`/`jpeglib`. The SDK contract should evolve toward requesting host-provided writable texture buffers for text or other generated runtime textures, then returning textured geometry batches.
- Viewer-side shader code is allowed only inside backend targets such as `figma_viewer`; SDK shader descriptors remain out of scope until a decoded Figma feature requires a public shader contract.
- Unsupported Figma/prototype features must appear in diagnostics/coverage reports. Runtime/viewer must not draw guessed visual fallbacks for unsupported Figma data; the default policy is skip the command and emit a diagnostic with the node id and feature name when available.
- Runtime/viewer behavior must be data-driven from decoded Figma fields, `.ux.json` bindings/actions, host callbacks, and explicit viewer controls. Do not branch on concrete sample node ids, layer names, visible text, asset ids, or document names to repair visuals or prototype behavior.
- `figma_sdk` uses allocator-aware C strings, arrays, maps/sets, and byte buffers. All SDK-owned allocations, including dependency allocation hooks where available, route through the runtime allocator. Failures use `figma_result_t` plus explicit cleanup and transactional publication: malformed input or OOM cannot publish partial objects, and a failed `figma_document_load_ux` preserves the previous mappings.
- Number serialization uses a locale-independent shortest-roundtrip C11 formatter.
- The active product build includes `figma_sdk`, the Objective-C++17 `figma_viewer`, the C11 `figma_dump` developer inspection tool behind `FIGMA_TOOLS`, and CTest coverage behind `FIGMA_TESTS`. `FIGMA_TESTS` defaults on for standalone builds and off when embedded.

## 3. C11 And Viewer Style

- SDK and `figma_dump` files use lowercase snake_case names, `figma_*_t` types, `FIGMA_*` constants, explicit ownership, and explicit result handling.
- C function declarations and definition signatures stay on one physical line. C implementation functions are separated with `//////////////////////////////////////////////////////////////////////////`, and file-local `static` function names use a `__` prefix.
- SDK code is strictly C11: no C++ sources under `sdk`, no STL, exceptions, RTTI, templates, namespaces, classes, or hidden C++ runtime dependency.
- SDK-owned dynamic storage uses runtime-allocator-aware C containers. All allocation-size arithmetic is checked and failure paths release temporary state through cleanup blocks.
- Public structs are plain C aggregates. Public enums have explicit numeric values and ABI-visible counts/flags use fixed-width integer fields.
- Functions return `figma_result_t` when an operation can fail; destroy functions accept their exact opaque handle type and return `void`.
- New state is built transactionally and published only after successful parsing/allocation. Unsupported decoded behavior is reported through diagnostics rather than silently guessed.
- Objective-C++17 and STL remain allowed only in `figma_viewer` backend code. Viewer code follows the established Mengine-style formatting and keeps its render-command snapshots outside the SDK ABI.

## 4. SDK Surface

Public opaque handles:

```c
figma_runtime_t
figma_document_t
figma_player_t
figma_render_list_t
figma_diagnostics_t
```

Primary creation and destruction shape:

```c
figma_result_t figma_runtime_create(uint32_t version, const figma_runtime_desc_t *desc, figma_runtime_t **runtime);
void figma_runtime_destroy(figma_runtime_t *runtime);

figma_result_t figma_runtime_load_document_from_fig_data(figma_runtime_t *runtime, const void *data, size_t size, const figma_load_options_t *options, figma_document_t **document);
void figma_document_destroy(figma_document_t *document);

figma_result_t figma_runtime_create_player(figma_runtime_t *runtime, figma_document_t *document, const figma_player_desc_t *desc, figma_player_t **player);
void figma_player_destroy(figma_player_t *player);
```

`figma_document_load_ux` replaces binding/action mappings atomically. Asset lookup and diagnostics return borrowed descriptors/views owned by the document.

`figma_action_router_t` and `figma_data_context_t` are callback tables with `user_data`; the player copies the tables but never owns their `user_data`. Binding values and action responses use typed C descriptors and `figma_string_view_t`.

Player operations include viewport setup, hit testing, pointer/key input, update/restart, frame/overlay/history navigation, typed binding overrides, and read-only render-list/diagnostics access. `figma_player_desc_t.start_frame_id` may select a decoded frame for tools and host-controlled entry points; an empty view starts at decoded `prototypeStartNodeID` / `prototypeStartingPoint`.

`figma_render_list_t` is a read-only host rendering surface:

```c
uint32_t figma_render_list_get_batch_count(const figma_render_list_t *render_list);
figma_result_t figma_render_list_get_batch(const figma_render_list_t *render_list, uint32_t index, figma_render_batch_desc_t *batch);
```

The public API does not expose mutable render-list construction, decoded canvas/prototype nodes, animation-track storage, `.ux.json` mapping storage, or concrete implementation objects. Repository-owned dump/viewer tooling includes the private C count/get inspection header `sdk/src/figma_inspection.h`; it does not cast opaque handles to concrete SDK objects.

## 5. Binding And Actions

The primary binding/action model is an external `.ux.json` file next to the `.fig` file. The host loads that file or equivalent resource data and passes the UX data to `figma_document_load_ux()`.

The `.ux.json` data maps stable Figma node/component ids to:

- game-facing binding keys;
- binding property names such as `text`, `visible`, `enabled`, `selected`, or `image`;
- semantic action ids;
- optional target frame ids for navigation.
- optional input triggers; omitted triggers remain click actions for backward compatibility.

The host provides data through `figma_data_context_t` or by pushing per-key overrides through `figma_player_set_text`, `figma_player_set_number`, `figma_player_set_visible`, `figma_player_set_enabled`, `figma_player_set_image`, `figma_player_set_state`, or `figma_player_set_binding_value`. `figma_player_clear_binding_value` removes a manual override so the value can fall back to the data-context callback. `figma_player_update` pulls current binding values and refreshes the render list. Binding updates may only mutate existing decoded nodes; missing nodes or text/image layout that cannot be resolved from decoded node data produce diagnostics instead of demo render commands. `visible=false` hides the node from rendering and interaction; `enabled=false` keeps rendering intact but suppresses generated prototype/action hotspots for that node.

Each successful `figma_document_load_ux()` call atomically replaces the previous binding/action mappings. Invalid or out-of-memory UX loads leave the previous mappings intact and return an explicit `figma_result_t`.

The host handles prototype triggers, game actions, and playback lifecycle through `figma_action_router_t`. Runtime reports each decoded trigger before routing its ordered actions, then emits typed events with action id, interaction id, source/current/target ids, trigger/navigation/connection types, pointer/key/timer context, and the `user_data` pointer from `figma_player_desc_t`. Event string fields are `figma_string_view_t` values valid during the synchronous callback. Callback results can allow default behavior, consume the current action, navigate, open an overlay, or close an overlay; `figma_action_response_t.target_frame_id` is consumed before `route_action` returns. Frame, overlay, and component-state callbacks are emitted after the corresponding playback state changes.

Prototype hit areas are SDK-owned transformed quads clipped by decoded ancestor frames. Pointer capture is keyed by pointer id and button, and a click requires a matching down/up sequence on the same internal hotspot. Hosts integrate the whole player as one picker or input surface; they must not materialize decoded Figma hotspots as host scene nodes.

## 6. Viewer

`figma_viewer` is an Objective-C++17 macOS/AppKit bundle built only on Apple platforms. It includes the public C ABI through `figma/figma.hpp`, may use STL inside its own backend, and accepts either a local `.fig` path argument or a `File > Open...` menu selection from an `NSOpenPanel`. The viewer owns local file IO, reads `.fig`/`.ux.json` data itself, and passes memory views/string views into the SDK. It starts in prototype viewport mode by default: the document is rendered inside a centered rectangular screen viewport using the decoded prototype frame size/aspect when available, the final viewport is rendered into a `CAMetalLayer` from SDK render-list geometry/textures/text bitmaps, animations are enabled by default, the viewer fast-forwards through the first decoded timed intro navigation on the prototype start frame so the default window opens on the first post-intro playable screen while the SDK still supports full start-frame playback, input is remapped into viewport coordinates, pointer down/move/up events are forwarded through `figma_player_input_pointer`, a timer calls `figma_player_update` for timed prototype actions and active animations while the window is visible and playback is running, the active render loop is capped to 30 FPS and sleeps until the next frame when a tick arrives early, hidden/miniaturized/occluded viewer windows and paused playback stop render-list updates and switch to a 100 ms idle timer until visible/running again, the viewer exposes Run/Pause controls, a discrete slowdown-only speed slider from `1/100x` through `1x`, and paused frame step controls where each step uses `1/60 * speed` seconds of SDK playback time, backward stepping is implemented by replaying the viewer timeline from the post-intro restart baseline through recorded pointer events rather than by approximating reverse animation in the viewer, the restart button calls `figma_player_restart`, reapplies the same decoded intro fast-forward, and leaves playback paused on the first post-intro animation frame until Run is pressed, the viewer camera supports Space-held wheel zoom, Space-held mouse drag pan, and `Esc` reset around the simulated screen, the viewer exposes an explicit Normal/Wireframe/Combined render mode control, the viewer can show a right-side render-list inspector with current command type/id/node data, per-command visibility toggles, expandable command details for decoded render properties, document/asset paths, image UV/vertices, image filter arrays, and text/font/line metadata, and a copy action that puts the selected command properties on the clipboard for debugging SDK output, configured and system fonts are resolved directly by FreeType from decoded Figma font family/style/PostScript metadata without guessed font substitution or committed sample-font fallbacks and rasterized into Metal textures at the current destination pixel scale before being placed back into Figma units, missing decoded font requests are listed after document load with an option to add a user-selected `.ttf`/`.otf`/`.ttc` font directory for the current viewer session, image commands are uploaded as Metal textures with mipmaps and drawn from SDK-provided geometry/UV and decoded paint metadata, decoded image filter fields supported by the viewer are applied from the render command before texture upload rather than node-name heuristics, SDK render-layer opacity is composed through Metal offscreen textures at native `backingScaleFactor` pixel size rather than a 1x logical framebuffer, decoded normal/multiply/screen/overlay/darken/lighten/color-dodge/color-burn/soft-light/hard-light/difference/exclusion blend modes are composed by the Metal backend, debug hotspot overlay is hidden by default and toggled with `H`, and diagnostics are shown outside the simulated screen or inside viewer tooling panels without reserving empty layout space. The viewer must not tint template-like icons, lay out Figma text, clip images, or display thumbnails/previews as guessed rendering behavior unless those operations are backed by decoded Figma data and represented in the render list.

It must:

- open a local `.fig`;
- optionally load a `.ux.json` sidecar;
- show the decoded prototype start frame from `prototypeStartNodeID` / `prototypeStartingPoint`;
- display render-list output from decoded canvas nodes, fills, strokes, image geometry/UV, text lines, diagnostics, and hotspot/wireframe overlays;
- expose viewer-only render-list inspection controls and expandable command property details without mutating SDK document/player state;
- render SDK-provided mesh geometry for Figma shapes instead of rebuilding those shapes through AppKit bezier APIs;
- keep Metal rendering constrained to `figma_viewer`; `figma_sdk` remains backend-free;
- route AppKit input into `figma_player_t`.

Example:

```bash
sh build/downloads/downloads.sh
sh build/xcode_macos/make_solution_xcode_macos.sh Debug
sh build/xcode_macos/build_solution_xcode_macos.sh Debug figma_viewer
open -n "$PWD/solutions/bin/xcode_macos/Debug/figma_viewer.app" --args /Users/yurii.levchenko/Downloads/KROSSROAD_Presentation.fig
```

## 7. Developer Tools

`figma_dump` is a C11 read-only developer inspection CLI built when `FIGMA_TOOLS=ON`. It reads a local `.fig`, optionally reads a `.ux.json` sidecar, passes the loaded data into the SDK, prints JSON with document metadata, diagnostics, prototype start frame, selected node matches including decoded font metadata, decoded fill/stroke geometry path ids and resolved path paints, optional render-list summaries for the prototype start or an explicit `--frame` id, and `--animations` prototype interaction/action/transition inspection. It must not implement preview/fallback rendering and uses the public C API plus the private C count/get inspection header rather than private viewer behavior or concrete casts.

## 8. Current Implementation Slice

The current first slice parses host-provided `.fig` archive bytes through a small zlib-backed ZIP reader, parses `meta.json` including `thumbnail_size`, extracts thumbnail/assets with PNG size metadata, validates `canvas.fig` prefix/version, decodes the embedded Kiwi schema and scene data for a first subset of Figma nodes through `thirdparty/kiwi` binary primitive reads plus SDK-owned Figma schema dispatch, keeps decoded `symbolData.symbolID` on normalized nodes and expands simple component instances through it, selects a prototype start frame only from decoded `prototypeStartNodeID` / `prototypeStartingPoint`, decodes `prototypeInteractions` for click, hover enter/leave, press, pointer down/up, timeout, and key-down triggers, decodes currently supported transition metadata including `DISSOLVE`, `SMART_ANIMATE`, `IN_CUBIC`, `OUT_CUBIC`, `LINEAR`, duration, preserve-scroll and reset-video flags, runs SDK-owned active animation state through `figma_player_update`, reports decoded triggers to the host before routing every ordered interaction action, runs nested component `SWAP_STATE` interactions as local node state changes with their own timeout origin and transition timing instead of navigating the whole player to off-canvas component variants, carries timeout overshoot into newly started prototype animations so large `dt` values do not add visible start delay, exposes `figma_player_restart` for replaying the decoded prototype start frame without recreating the document/player bindings, renders `DISSOLVE` as complementary source and target render-list layers that cross-fade, while keeping matched persistent leaves outside the fading layers at full opacity so shared UI does not double-render or flash, runs Smart Animate through stable node-id matching and deterministic sibling layer matching by decoded node type/name for duplicated Figma frames, stores Smart Animate source/target node ids, interpolates rect tracks in frame-local coordinates so off-canvas artboard placement does not affect playback, draws visually identical matched leaf layers from the source node at their target stacking position and suppresses duplicate target geometry so unchanged persistent UI never fades or flashes at completion, traverses matched source containers so unmatched descendants can still fade out, and keeps Smart Animate root frames out of full-screen source/target opacity fades so matched layers animate without whole-screen washout, skips guessed visual animation with diagnostics when required animation data is not decoded, loads `.ux.json` bindings/actions from host-provided UX data, decodes `NodeChange.mask`, `maskType`, `frameMaskDisabled`, node-level `blendMode`, node transformed quad corners, `fillGeometry`, `strokeGeometry`, `vectorData`, `VectorData.styleOverrideTable` entries used by `Path.styleID`, `styleIdForFill`/`styleIdForStrokeFill` paint style references, `arcData`, stroke align/cap/join/dash metadata, `Paint.blendMode`, `imageScaleMode`, image transform mapped into render-list UV coordinates, filter fields, and original image size, resolves `Path.commandsBlob` through decoded `Message.blobs`, produces render-list commands for image fills with SDK-provided textured quad geometry/UV including decoded axis-aligned mirror transforms and a half-texel inset toward source texel centers, and for text with decoded lines plus decoded solid text fill opacity and blend metadata, produces `irov/graphics` generated mesh commands for solid fills on frames, rectangles, rounded rectangles, and ellipses, primitive strokes from decoded primitive stroke metadata for frames, rectangles, rounded rectangles, and ellipses, decoded filled ellipse arc data through state-based `gp_set_inner_radius` plus `gp_begin_fill`/`gp_ellipse_arc`/`gp_end_fill`, skips zero-width decoded strokes without unsupported-feature diagnostics, bakes decoded primitive stroke alignment into generated mesh geometry without clamping positive stroke widths upward, uses decoded `strokeGeometry` as pre-expanded filled stroke outlines through `irov/graphics` for non-primitive/vector geometry instead of applying `strokeWeight` a second time, produces decoded vector/path fill geometry with path-level style paints, groups path-level solid fills for a vector node into one mesh when a node-level blend mode such as ColorBurn must apply to the node result rather than each internal path, forwards decoded normal/multiply/screen/overlay/darken/lighten/color-dodge/color-burn/soft-light/hard-light/difference/exclusion/hue/saturation/color/luminosity blend modes to render commands, prefers primitive shape fields over serialized vector fill metadata for frame, rectangle, rounded rectangle, and ellipse fills, preserves decoded image filter metadata in image commands, applies decoded `filterColorAdjust`/`paintFilter` tint, shadows, highlights, exposure, temperature, vibrance, contrast, and brightness in the viewer before Metal texture upload, samples decoded image fills directly from render-list UVs in the Metal shader, uploads image textures with mipmaps for minification, composes SDK layer opacity through Metal offscreen textures, emits diagnostics for unsupported decoded `filterColorAdjust` and `paintFilter` fields, reads text font family/style/PostScript name and line-height metadata, keeps derived text baseline data for viewer baseline placement, rasterizes Figma text through FreeType glyph bitmaps from configured or system fonts only when the face matches decoded font metadata and uploads the text result as a Metal texture at the active destination pixel scale, keeps `gp_arc`, `gp_arc_to`, `gp_ellipse_arc`, `gp_ellipse_arc_to`, and `gp_set_inner_radius` available in `irov/graphics` for decoded vector/path work, lets the viewer resolve missing `.fig` paths to adjacent `.fig.zip` exports, includes `figma_dump --animations` for JSON inspection of decoded prototype animation data, and routes native and `.ux.json` actions through transformed, clipped SDK-owned hotspot quads.

During a full-frame dissolve, the source pass preserves the currently selected nested `SWAP_STATE` variants; it must not reconstruct the source screen from component defaults before applying transition opacity.

Prototype interaction support comprises click, hover enter/leave, press, pointer down/up, timeout, and key-down triggers; ordered navigate, swap, overlay, back, and close actions; pointer capture; transformed/clipped hotspot hit-testing; navigation history; centered overlay composition; and host trigger/action/lifecycle callbacks. Instant, dissolve, Smart Animate, move, push, and slide transitions are SDK-owned playback behavior.

Unsupported node types, `vectorNetworkBlob` path reconstruction, compound vector fill holes/winding beyond the current contour support, gradients, effects, unsupported blend modes, unsupported image filter fields such as detail/vignette and exact Figma image-filter shader matching, exact image-mask texture meshes, host texture allocation callbacks for generated text, constraints, exact text shaping, custom easing functions, drag/scroll/external-URL prototype actions, exact overlay metadata beyond centered composition, and full Figma Smart Animate matching beyond decoded id plus sibling type/name matching remain roadmap work and must stay visible in diagnostics/coverage as support expands.

## 9. Build And Verification

- CMake preserves target `figma_sdk` and alias `Figma::SDK`, supports `STATIC` and `SHARED`, requires C11 with extensions disabled, and hides C symbols by default except `FIGMA_API` entrypoints.
- The root project enables only C until a selected viewer/test target needs Objective-C++17 or C++17 for the adapter probes.
- `figma_dump` is compiled as C11. `figma_viewer` remains Objective-C++17 and links the C SDK through `figma.hpp`.
- `FIGMA_TESTS` adds the permanent C11 `figma_sdk_tests` CTest target plus C11, C++17, and Apple Objective-C++17 include/linkage probes. C++ probes include only `figma/figma.hpp`; direct C++ inclusion of `figma.h` is not a supported contract.
- Synthetic-document tests cover argument/version failures, allocator fault injection and cleanup, callbacks, bindings, navigation, pointer capture, animation, render-list publication, atomic UX replacement, and locale-independent number formatting.
- Release validation compares `figma_dump` JSON against the known fixture for default, `--render-list`, `--animations`, `--find`, and `--node`; builds Debug SDK/dump/viewer/tests; runs CTest and first-party ASan/UBSan through `FIGMA_SANITIZERS`; checks static/shared SDKs; and verifies that SDK symbols/dependencies contain no C++ runtime surface.
