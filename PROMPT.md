# Figma SDK Project Prompt

This file is the durable product prompt and source of requirements for this repository. Update it in the same change whenever architecture, public API, `.fig` import, binding schema, viewer UX, build rules, tests, or roadmap change.

## 1. Purpose

`figma` is a native C++ SDK and viewer for playing local Figma UI/UX exports in host applications.

The SDK parses host-provided Figma Desktop `.fig` export bytes, keeps document and playback state, handles prototype input, data binding, and host callbacks, then returns a backend-free render list. The host engine or viewer owns filesystem/resource IO and the actual rendering.

The first verification fixture is:

```text
/Users/yurii.levchenko/Downloads/KROSSROAD_Presentation.fig
```

That file is a ZIP archive containing `meta.json`, `thumbnail.png`, `images/*`, and binary `canvas.fig` with a `fig-kiwi` prefix.

## 2. Core Decisions

- SDK language is C++17.
- SDK target name is `figma_sdk`.
- Public namespace is `Figma`.
- CMake is the canonical build system.
- The repository root CMake project is reusable through `add_subdirectory`. Standalone builds use downloaded bundled dependencies and enable the viewer/tools by default; embedded builds disable those defaults and receive host-created dependency targets, any legacy non-transitive dependency include directories, optional private compile definitions, and the requested `STATIC` or `SHARED` SDK library type through `FIGMA_*` parameters. Hosts link the stable `Figma::SDK` alias and must not maintain a duplicate `figma_sdk` source list.
- Thirdparty source acquisition follows the Mengine-style downloads project: run `sh build/downloads/downloads.sh` to configure/build the downloads solution under `solutions/downloads` and populate `thirdparty/*`. The repository must not rely on Git submodules for normal dependency setup.
- `build/` is a script-entrypoint directory only and is grouped by build family, for example `build/downloads/` and `build/xcode_macos/`; future MSVC or other toolchain entrypoints must get their own subdirectories. Generated CMake/Xcode solutions, binaries, and app bundles live under ignored `solutions/*` paths.
- `.fig` archive bytes are the primary v1 input; no Figma API or network auth is required. SDK code does not open filesystem paths or own other system IO. Host tools, viewers, or engine integrations load resources through their own IO layer and pass `.fig` data as a raw pointer plus byte size and optional `.ux.json` strings into the SDK.
- SDK is backend-agnostic and must not depend on AppKit, Electron, DOM, CSS, OpenGL, Metal, Vulkan, or DirectX.
- SDK public headers under `sdk/include/Figma` expose only host-facing integration API declarations, public descriptors, and pure virtual runtime/document/player/render/binding/action interfaces. Interfaces use protected non-virtual destructors so callers cannot delete through a base interface pointer. SDK-created objects are returned as raw interface pointers and are released explicitly through interface `destroy()` methods rather than through public virtual destructors or shared ownership; host callback interfaces are borrowed pointers and are never destroyed by the SDK. Private storage, concrete runtime classes, concrete decoded canvas/prototype model descriptors, inspection interfaces, helper declarations, decoder/loader internals, and backend-independent implementation headers live under `sdk/src`.
- Public and SDK-private interface objects are passed, returned, and stored as pointers, not references, so optional/null state and ownership remain explicit across integration boundaries.
- The public binary entrypoint is only `FIGMA_EXPORT createRuntime(...)`; loading documents, UX data, and players is routed through virtual interfaces from the created runtime/document objects so DLL builds do not expose extra creation functions.
- The public header defines `FIGMA_SDK_VERSION`; hosts pass it as the first argument to `createRuntime(...)`, which compares it with the value compiled into the library and returns `EResult::VersionMismatch` without reading the descriptor or creating a runtime when they differ. Increment `FIGMA_SDK_VERSION` for every SDK source or public API change before producing integration builds.
- SDK returns render lists as read-only render batches: geometry vertices/indices plus texture or asset references where applicable; host/viewer uploads resources and renders them. Public clients enumerate `RenderListInterface` through `getBatchCount()` / `getBatch(index, RenderBatchDesc*)`; the SDK fills the caller-provided `RenderBatchDesc` with only batch type, shader type, texture type/key, blend/opacity/layer state, and mesh buffers. Figma-specific command metadata such as node ids, text/font fields, shape descriptors, decoded image filters, and mutable render-command builders are SDK-private implementation details under `sdk/src`.
- SDK owns prototype animation timing and composition. Viewer/backend code calls `PlayerInterface::update(dt)` and renders the SDK-produced `RenderListInterface`; it must not implement Figma-specific tweening or navigation animation logic.
- Geometry generation uses `irov/graphics`. Viewer/backend code must not construct Figma shape geometry with AppKit/CoreGraphics bezier helpers; if `irov/graphics` lacks a required primitive, extend `irov/graphics` instead of adding backend-only or SDK-local geometry. `irov/graphics` rendering must preserve primitive insertion order across primitive types so Figma z-order is not lost during mesh generation.
- JSON parsing and `.ux.json` reading use `irov/json`.
- Kiwi binary primitive reading uses `irov/kiwi`, a small pure-C thirdparty library under `thirdparty/kiwi`; its primitive reader API returns values directly and uses assertions for reader bounds/varint invariants, while Figma-specific schema interpretation, external file validation, diagnostics, and normalized document construction stay in `figma_sdk`.
- ZIP reading uses system `zlib`; the SDK owns the minimal ZIP container parser needed for host-provided `.fig` archive bytes.
- `canvas.fig` Kiwi chunks are decompressed with raw deflate through `zlib` and required `libzstd` when scene data uses Zstandard.
- The first native viewer is a macOS AppKit shell with a Metal-backed Figma viewport and lives outside the SDK. AppKit owns the window, controls, input routing, and debug/inspector UI; Metal owns final viewport composition.
- Viewer text rasterization uses downloaded FreeType from `thirdparty/freetype` and resolves font files from decoded Figma font metadata through an optional `FIGMA_VIEWER_FONT_DIRS` font collection path plus local system font directories; the viewer and SDK must not commit or bundle sample-specific `.fig` fonts. Missing fonts are reported by the viewer instead of guessed through bundled fallbacks, then generated text bitmaps are uploaded as Metal textures when a matching face is found. PNG image assets are decoded through downloaded `libpng`; JPEG image assets are decoded through downloaded `libjpeg`/`jpeglib`. The SDK contract should evolve toward requesting host-provided writable texture buffers for text or other generated runtime textures, then returning textured geometry batches.
- Viewer-side shader code is allowed only inside backend targets such as `figma_viewer`; SDK shader descriptors remain out of scope until a decoded Figma feature requires a public shader contract.
- Unsupported Figma/prototype features must appear in diagnostics/coverage reports. Runtime/viewer must not draw guessed visual fallbacks for unsupported Figma data; the default policy is skip the command and emit a diagnostic with the node id and feature name when available.
- Runtime/viewer behavior must be data-driven from decoded Figma fields, `.ux.json` bindings/actions, host callbacks, and explicit viewer controls. Do not branch on concrete sample node ids, layer names, visible text, asset ids, or document names to repair visuals or prototype behavior.
- The active product build includes `figma_sdk`, `figma_viewer`, and the explicitly requested `figma_dump` developer inspection tool behind `FIGMA_TOOLS`. Standalone test executables remain out of scope unless explicitly requested again; diagnostics should remain available through SDK/player/viewer surfaces.

## 3. C++ Style

C++ code follows the Mengine codestyle:

- The repository root `.clang-format` captures the mechanically enforceable subset of the Mengine style and is applied to first-party C++ SDK sources; semantic rules such as one primary class per file and implementation dividers remain review requirements.
- PascalCase for files and public types.
- `E*` prefix for enums.
- `Desc` suffix for descriptor structs.
- `_arg` names for function arguments.
- `m_` prefix for member fields.
- `#pragma once` for headers.
- The related header is the first include in each `.cpp`.
- Concrete classes use `protected:` for internal methods and fields unless a member must be strictly inaccessible to derived/friend SDK internals; avoid broad `private:` sections in SDK/viewer classes.
- SDK-private inspection keeps virtual interfaces only at real integration entrypoints such as document inspection. Decoded node and leaf data such as nodes, paints, paths, prototype actions/interactions, and text lines stays in concrete descriptor types instead of one-off virtual interfaces unless polymorphic replacement or a real integration boundary requires that abstraction.
- SDK-private concrete decoded descriptors expose their fields directly and do not add `get*`/`set*` wrappers that duplicate public member access.
- Descriptor structs are data holders: non-constructor behavior such as lookup, search, conversion, or mutation helpers belongs in named free functions or real classes, not as public member methods on structs. PMR constructors for owning `FigmaString`/`FigmaVector` fields are allowed.
- SDK implementation headers under `sdk/src` are named after their primary class or domain and must not use a `Private` suffix merely to indicate non-public visibility.
- Concrete non-interface C++ classes live in matching PascalCase `.h` and `.cpp`/`.mm` pairs, one primary class per pair, for example `Player` is declared in `Player.h` and implemented in `Player.cpp`; `.cpp`/`.mm` files contain method definitions and file-scope helper functions, not primary class declarations.
- Implementation files separate function, method, helper struct, and helper type alias definitions with the Mengine divider comment `//////////////////////////////////////////////////////////////////////////` at the current namespace/objective-c implementation indentation level, including a final divider before closing the implementation namespace. Dividers separate implementation-scope definitions; do not put them between member declarations inside local helper structs.
- SDK implementation helpers under `namespace Figma` use a nested `namespace Detail` rather than anonymous namespaces. File-local helper functions inside `Detail` should keep internal linkage with `static` when they are not part of a cross-translation-unit contract.
- Indexed loops over container contents cache the container size in a local `const std::size_t` variable with a domain name such as `definitionSize`, `fieldSize`, or `commandSize` instead of calling `.size()` in the loop condition.
- Public API returns explicit `EResult`/status values rather than throwing exceptions across the SDK boundary.
- Public API descriptors, SDK-owned descriptors, and SDK implementation headers must use Mengine-style SDK aliases for STL/PMR template types. Base aliases such as `FigmaAllocator<T>`, `FigmaVector<T>`, `FigmaUnorderedMap<K, V>`, `FigmaUnorderedSet<T>`, `FigmaString`, `FigmaStringView`, `FigmaMemoryResource`, `FigmaByteBuffer`, and `FigmaGetDefaultMemoryResource` live in `Types.h`; domain aliases such as `DiagnosticVector` or `BindingVector` must be built from those aliases instead of spelling `std::pmr::vector<T>`, `std::pmr::unordered_map<K, V>`, or `std::pmr::string` directly outside the base alias declarations.
- `Types.h` is for low-level shared SDK types only. API-domain descriptors live next to their API surface: runtime allocation options live with `RuntimeInterface`, `.fig` loading options live with `DocumentInterface`, while player viewport/input descriptors live with `PlayerInterface` and are consumed by `PlayerInterface`/`ActionRouterInterface`. Low-level standard numeric includes such as `<cstdint>` are centralized through `Types.h` for public SDK headers.
- Public value descriptors that contain `FigmaString` or `FigmaVector` must have inline constructors in public headers, or avoid owning SDK containers entirely. Public callback event payloads should prefer `FigmaStringView` when the data is only valid for the synchronous callback scope.
- Low-level numeric structs such as `Vec2f`, `Rectf`, and `Color` stay aggregate structs without user-defined constructors; callers use brace initialization like `Color{1.0f, 1.0f, 1.0f, 1.0f}`.
- Public input descriptors use typed enums: pointer buttons use `EPointerButton`, and keyboard/pointer modifiers use `EInputModifierFlag` bit flags.

## 4. SDK Surface

Core classes:

```text
RuntimeInterface
DocumentInterface
PlayerInterface
RenderListInterface
AssetProviderInterface
DataContextInterface
ActionRouterInterface
DiagnosticsInterface
```

Primary API shape:

```cpp
FIGMA_EXPORT EResult createRuntime(std::uint32_t _version, const RuntimeDesc& _desc, RuntimeInterface** const _runtime);
```

`RuntimeInterface` exposes document/player creation through virtual methods:

```cpp
EResult loadDocumentFromFigData(const void* _data, std::size_t _size, const LoadOptions& _options, DocumentInterface** const _document);
EResult createPlayer(DocumentInterface* const _document, const PlayerDesc& _desc, PlayerInterface** const _player);
```

`DocumentInterface` exposes:

```cpp
EResult loadUX(FigmaStringView _data);
const DiagnosticsInterface* getDiagnostics() const;
const AssetDesc* findAsset(FigmaStringView _assetId) const;
```

`DataContextInterface` exposes host-owned binding values and dirty state as a pure virtual contract:

```cpp
bool getBindingValue(FigmaStringView _key, BindingValue* const _value);
bool isBindingDirty(FigmaStringView _key) const;
```

Decoded canvas/prototype tree inspection is not part of the public integration API. Private debug tooling may include SDK-private headers such as `sdk/src/DocumentInspection.h` / `sdk/src/Document.h` to inspect decoded nodes, document metadata, bindings, actions, and animation details.

`PlayerInterface` exposes:

```cpp
EResult setActionRouter(ActionRouterInterface* _router);
EResult setDataContext(DataContextInterface* _context);
EResult hitTest(float _x, float _y, bool* const _hit) const;
EResult inputPointer(const PointerEvent& _event, InputDispatchResult* const _dispatch = nullptr);
EResult inputKey(const KeyEvent& _event, InputDispatchResult* const _dispatch = nullptr);
EResult update(float _dt);
EResult restart();
EResult navigateToFrame(FigmaStringView _targetFrameId);
EResult openOverlay(FigmaStringView _targetFrameId);
EResult closeOverlay();
EResult goBack();
EResult setText(FigmaStringView _key, FigmaStringView _value);
EResult setNumber(FigmaStringView _key, double _value);
EResult setVisible(FigmaStringView _key, bool _value);
EResult setEnabled(FigmaStringView _key, bool _value);
EResult setImage(FigmaStringView _key, FigmaStringView _assetId);
EResult setState(FigmaStringView _key, bool _value);
EResult setBindingValue(FigmaStringView _key, const BindingValue& _value);
EResult clearBindingValue(FigmaStringView _key);
void destroy();
const RenderListInterface* getRenderList() const;
const DiagnosticsInterface* getDiagnostics() const;
```

`RenderListInterface` is a read-only host rendering surface:

```cpp
std::uint32_t getBatchCount() const;
EResult getBatch(std::uint32_t _index, RenderBatchDesc* const _batch) const;
```

The public API must not expose mutable render-list construction methods, concrete render command containers, decoded canvas/prototype inspection interfaces, animation-track descriptors, `.ux.json` binding/action descriptors, or Figma decoded node/text/image internals as public fields. Viewer/debug tools may include SDK-private inspection headers for developer panels, but engine integrations such as Mengine consume the public runtime/player/render/binding/action contract.

`PlayerDesc` may optionally provide a decoded `startFrameId` for tooling and host-controlled entry points. When it is empty, playback starts from decoded `prototypeStartNodeID` / `prototypeStartingPoint`.

## 5. Binding And Actions

The primary binding/action model is an external `.ux.json` file next to the `.fig` file. The host loads that file or equivalent resource data and passes the UX data to `DocumentInterface::loadUX()`.

The `.ux.json` data maps stable Figma node/component ids to:

- game-facing binding keys;
- binding property names such as `text`, `visible`, `enabled`, `selected`, or `image`;
- semantic action ids;
- optional target frame ids for navigation.
- optional input triggers; omitted triggers remain click actions for backward compatibility.

The host provides data through `DataContextInterface` or by pushing per-key overrides through `PlayerInterface::setText()`, `setNumber()`, `setVisible()`, `setEnabled()`, `setImage()`, `setState()`, or `setBindingValue()`. `clearBindingValue()` removes a manual override so the value can fall back to `DataContextInterface`. `PlayerInterface::update()` pulls current binding values and refreshes the render list. Binding updates may only mutate existing decoded nodes; missing nodes or text/image layout that cannot be resolved from decoded node data produce diagnostics instead of demo render commands. `visible=false` hides the node from rendering and interaction; `enabled=false` keeps rendering intact but suppresses generated prototype/action hotspots for that node.

Each successful `DocumentInterface::loadUX()` call atomically replaces the previous binding/action mappings. Invalid or out-of-memory UX loads leave the previous mappings intact and return an explicit `EResult`.

The host handles prototype triggers, game actions, and playback lifecycle through `ActionRouterInterface`. Runtime reports each decoded trigger before routing its ordered actions, then emits typed action events with action id, interaction id, source/current/target ids, trigger/navigation/connection types, pointer/key/timer context, and the `ud` user-data pointer from `PlayerDesc`. `ActionEvent` string fields are `FigmaStringView` values valid during the synchronous `routeAction()` callback. Callback results can allow default behavior, consume the current action, navigate, open an overlay, or close an overlay; `ActionResponse::targetFrameId` is also a synchronous `FigmaStringView` consumed before `routeAction()` returns to the caller. Frame, overlay, and component-state callbacks are emitted after the corresponding playback state changes.

Prototype hit areas are SDK-owned transformed quads clipped by decoded ancestor frames. Pointer capture is keyed by pointer id and button, and a click requires a matching down/up sequence on the same internal hotspot. Hosts integrate the whole player as one picker or input surface; they must not materialize decoded Figma hotspots as host scene nodes.

## 6. Viewer

`figma_viewer` is a macOS/AppKit bundle built only on Apple platforms and accepts either a local `.fig` path argument or a `File > Open...` menu selection from an `NSOpenPanel`. The viewer owns local file IO, reads `.fig`/`.ux.json` data itself, and passes memory views/string views into the SDK. It starts in prototype viewport mode by default: the document is rendered inside a centered rectangular screen viewport using the decoded prototype frame size/aspect when available, the final viewport is rendered into a `CAMetalLayer` from SDK render-list geometry/textures/text bitmaps, animations are enabled by default, the viewer fast-forwards through the first decoded timed intro navigation on the prototype start frame so the default window opens on the first post-intro playable screen while the SDK still supports full start-frame playback, input is remapped into viewport coordinates, pointer down/move/up events are forwarded to `Player`, a timer calls `Player::update(dt)` for timed prototype actions and active animations while the window is visible and playback is running, the active render loop is capped to 30 FPS and sleeps until the next frame when a tick arrives early, hidden/miniaturized/occluded viewer windows and paused playback stop render-list updates and switch to a 100 ms idle timer until visible/running again, the viewer exposes Run/Pause controls, a discrete slowdown-only speed slider from `1/100x` through `1x`, and paused frame step controls where each step uses `1/60 * speed` seconds of SDK playback time, backward stepping is implemented by replaying the viewer timeline from the post-intro restart baseline through recorded pointer events rather than by approximating reverse animation in the viewer, the restart button calls `Player::restart()`, reapplies the same decoded intro fast-forward, and leaves playback paused on the first post-intro animation frame until Run is pressed, the viewer camera supports Space-held wheel zoom, Space-held mouse drag pan, and `Esc` reset around the simulated screen, the viewer exposes an explicit Normal/Wireframe/Combined render mode control, the viewer can show a right-side render-list inspector with current command type/id/node data, per-command visibility toggles, expandable command details for decoded render properties, document/asset paths, image UV/vertices, image filter arrays, and text/font/line metadata, and a copy action that puts the selected command properties on the clipboard for debugging SDK output, configured and system fonts are resolved directly by FreeType from decoded Figma font family/style/PostScript metadata without guessed font substitution or committed sample-font fallbacks and rasterized into Metal textures at the current destination pixel scale before being placed back into Figma units, missing decoded font requests are listed after document load with an option to add a user-selected `.ttf`/`.otf`/`.ttc` font directory for the current viewer session, image commands are uploaded as Metal textures with mipmaps and drawn from SDK-provided geometry/UV and decoded paint metadata, decoded image filter fields supported by the viewer are applied from the render command before texture upload rather than node-name heuristics, SDK render-layer opacity is composed through Metal offscreen textures at native `backingScaleFactor` pixel size rather than a 1x logical framebuffer, decoded normal/multiply/screen/overlay/darken/lighten/color-dodge/color-burn/soft-light/hard-light/difference/exclusion blend modes are composed by the Metal backend, debug hotspot overlay is hidden by default and toggled with `H`, and diagnostics are shown outside the simulated screen or inside viewer tooling panels without reserving empty layout space. The viewer must not tint template-like icons, lay out Figma text, clip images, or display thumbnails/previews as guessed rendering behavior unless those operations are backed by decoded Figma data and represented in the render list.

It must:

- open a local `.fig`;
- optionally load a `.ux.json` sidecar;
- show the decoded prototype start frame from `prototypeStartNodeID` / `prototypeStartingPoint`;
- display render-list output from decoded canvas nodes, fills, strokes, image geometry/UV, text lines, diagnostics, and hotspot/wireframe overlays;
- expose viewer-only render-list inspection controls and expandable command property details without mutating SDK document/player state;
- render SDK-provided mesh geometry for Figma shapes instead of rebuilding those shapes through AppKit bezier APIs;
- keep Metal rendering constrained to `figma_viewer`; `figma_sdk` remains backend-free;
- route AppKit input into `Player`.

Example:

```bash
sh build/downloads/downloads.sh
sh build/xcode_macos/make_solution_xcode_macos.sh Debug
sh build/xcode_macos/build_solution_xcode_macos.sh Debug figma_viewer
open -n "$PWD/solutions/bin/xcode_macos/Debug/figma_viewer.app" --args /Users/yurii.levchenko/Downloads/KROSSROAD_Presentation.fig
```

## 7. Developer Tools

`figma_dump` is a read-only developer inspection CLI built when `FIGMA_TOOLS=ON`. It reads a local `.fig`, optionally reads a `.ux.json` sidecar, passes the loaded data into the SDK, prints JSON with document metadata, diagnostics, prototype start frame, selected node matches including decoded font metadata, decoded fill/stroke geometry path ids and resolved path paints, optional render-list summaries for the prototype start or an explicit `--frame` id, and `--animations` prototype interaction/action/transition inspection. It must not implement preview/fallback rendering and must use SDK public APIs or SDK-private inspection headers rather than private viewer behavior.

## 8. Current Implementation Slice

The current first slice parses host-provided `.fig` archive bytes through a small zlib-backed ZIP reader, parses `meta.json` including `thumbnail_size`, extracts thumbnail/assets with PNG size metadata, validates `canvas.fig` prefix/version, decodes the embedded Kiwi schema and scene data for a first subset of Figma nodes through `thirdparty/kiwi` binary primitive reads plus SDK-owned Figma schema dispatch, keeps decoded `symbolData.symbolID` on normalized nodes and expands simple component instances through it, selects a prototype start frame only from decoded `prototypeStartNodeID` / `prototypeStartingPoint`, decodes `prototypeInteractions` for click, hover enter/leave, press, pointer down/up, timeout, and key-down triggers, decodes currently supported transition metadata including `DISSOLVE`, `SMART_ANIMATE`, `IN_CUBIC`, `OUT_CUBIC`, `LINEAR`, duration, preserve-scroll and reset-video flags, runs SDK-owned active animation state through `Player::update(dt)`, reports decoded triggers to the host before routing every ordered interaction action, runs nested component `SWAP_STATE` interactions as local node state changes with their own timeout origin and transition timing instead of navigating the whole player to off-canvas component variants, carries timeout overshoot into newly started prototype animations so large `dt` values do not add visible start delay, exposes `Player::restart()` for replaying the decoded prototype start frame without recreating the document/player bindings, renders `DISSOLVE` as an opaque source render-list layer with the target layer fading over it, so the viewport background cannot leak through the cross-fade, while preserving persistent target roots at full opacity only when decoded `symbolId` or stable node id and frame-local geometry match, instead of per-node opacity or screen-specific persistent-layer matching, runs Smart Animate through stable node-id matching and deterministic sibling layer matching by decoded node type/name for duplicated Figma frames, stores Smart Animate source/target node ids, interpolates rect tracks in frame-local coordinates so off-canvas artboard placement does not affect playback, and keeps Smart Animate root frames out of full-screen source/target opacity fades so matched layers animate without whole-screen washout, skips guessed visual animation with diagnostics when required animation data is not decoded, loads `.ux.json` bindings/actions from host-provided UX data, decodes `NodeChange.mask`, `maskType`, `frameMaskDisabled`, node-level `blendMode`, node transformed quad corners, `fillGeometry`, `strokeGeometry`, `vectorData`, `VectorData.styleOverrideTable` entries used by `Path.styleID`, `styleIdForFill`/`styleIdForStrokeFill` paint style references, `arcData`, stroke align/cap/join/dash metadata, `Paint.blendMode`, `imageScaleMode`, image transform mapped into render-list UV coordinates, filter fields, and original image size, resolves `Path.commandsBlob` through decoded `Message.blobs`, produces render-list commands for image fills with SDK-provided textured quad geometry/UV including decoded axis-aligned mirror transforms and a half-texel inset toward source texel centers, and for text with decoded lines plus decoded solid text fill opacity and blend metadata, produces `irov/graphics` generated mesh commands for solid fills on frames, rectangles, rounded rectangles, and ellipses, primitive strokes from decoded primitive stroke metadata for frames, rectangles, rounded rectangles, and ellipses, decoded filled ellipse arc data through state-based `gp_set_inner_radius` plus `gp_begin_fill`/`gp_ellipse_arc`/`gp_end_fill`, skips zero-width decoded strokes without unsupported-feature diagnostics, bakes decoded primitive stroke alignment into generated mesh geometry without clamping positive stroke widths upward, uses decoded `strokeGeometry` as pre-expanded filled stroke outlines through `irov/graphics` for non-primitive/vector geometry instead of applying `strokeWeight` a second time, produces decoded vector/path fill geometry with path-level style paints, groups path-level solid fills for a vector node into one mesh when a node-level blend mode such as ColorBurn must apply to the node result rather than each internal path, forwards decoded normal/multiply/screen/overlay/darken/lighten/color-dodge/color-burn/soft-light/hard-light/difference/exclusion/hue/saturation/color/luminosity blend modes to render commands, prefers primitive shape fields over serialized vector fill metadata for frame, rectangle, rounded rectangle, and ellipse fills, preserves decoded image filter metadata in image commands, applies decoded `filterColorAdjust`/`paintFilter` tint, shadows, highlights, exposure, temperature, vibrance, contrast, and brightness in the viewer before Metal texture upload, samples decoded image fills directly from render-list UVs in the Metal shader, uploads image textures with mipmaps for minification, composes SDK layer opacity through Metal offscreen textures, emits diagnostics for unsupported decoded `filterColorAdjust` and `paintFilter` fields, reads text font family/style/PostScript name and line-height metadata, keeps derived text baseline data for viewer baseline placement, rasterizes Figma text through FreeType glyph bitmaps from configured or system fonts only when the face matches decoded font metadata and uploads the text result as a Metal texture at the active destination pixel scale, keeps `gp_arc`, `gp_arc_to`, `gp_ellipse_arc`, `gp_ellipse_arc_to`, and `gp_set_inner_radius` available in `irov/graphics` for decoded vector/path work, lets the viewer resolve missing `.fig` paths to adjacent `.fig.zip` exports, includes `figma_dump --animations` for JSON inspection of decoded prototype animation data, and routes native and `.ux.json` actions through transformed, clipped SDK-owned hotspot quads.

Prototype interaction support comprises click, hover enter/leave, press, pointer down/up, timeout, and key-down triggers; ordered navigate, swap, overlay, back, and close actions; pointer capture; transformed/clipped hotspot hit-testing; navigation history; centered overlay composition; and host trigger/action/lifecycle callbacks. Instant, dissolve, Smart Animate, move, push, and slide transitions are SDK-owned playback behavior.

Unsupported node types, `vectorNetworkBlob` path reconstruction, compound vector fill holes/winding beyond the current contour support, gradients, effects, unsupported blend modes, unsupported image filter fields such as detail/vignette and exact Figma image-filter shader matching, exact image-mask texture meshes, host texture allocation callbacks for generated text, constraints, exact text shaping, custom easing functions, drag/scroll/external-URL prototype actions, exact overlay metadata beyond centered composition, and full Figma Smart Animate matching beyond decoded id plus sibling type/name matching remain roadmap work and must stay visible in diagnostics/coverage as support expands.
