# OpenGenesisLINK Viewer

Native first-party Viewer for the OpenGenesisLINK virtual-world platform.

The Viewer is a separate project from the OpenGenesisLINK Server. The server remains authoritative; the Viewer integrates only through versioned Core HTTP and Scene contracts.

## Contract baseline

Development currently targets:

- OpenGenesisLINK Server `1.0.0-alpha.1`
- release discovery `ogl-release-v1`
- Core API version `1`
- Viewer bootstrap `ogl-viewer-bootstrap-v1`
- Scene application contract `scene-v2`
- OGL1 Scene transport protocol `1`

Unknown additive JSON fields are tolerated. Optional functionality is capability-detected rather than assumed.

## Current status — 0.10.0-dev

The first foundation block contains:

- cross-platform C++23 / CMake project layout
- Linux x86_64, Linux ARM64/aarch64 and Windows x86_64 CI
- HTTP transport abstraction with libcurl implementation
- `GET /v1` + `GET /v1/release` compatibility discovery
- Core login/session client
- Viewer bootstrap client
- released contract validation
- additive-field-safe JSON parsing
- unit tests with an injected fake transport
- `ogl-viewer-contract-smoke` compatibility utility
- byte-compatible OGL1 frame codec and bounded TCP stream decoder
- Scene v2 HELLO / SCENE_JOIN protocol validation
- initial SCENE_SYNC_REQUEST construction and request correlation
- portable native TCP Scene transport for Linux/ARM64 and Windows
- live HELLO → SCENE_JOIN → initial full-sync startup sequence
- deterministic GOODBYE/cleanup and deferred unrelated Scene frames
- authoritative SCENE_SYNC snapshot/delta parser
- Region/entity WorldModel with transforms, permissions, linksets and Physics state
- strict Scene sequence validation and snapshot-recovery signaling
- typed TERRAIN_SAMPLE requests and revision-aware terrain cache
- RenderRegion/RenderInstance projection from the authoritative WorldModel
- explicit box-proxy objects and avatar capsule proxies for the pre-render stage
- automatic full-snapshot recovery for unsafe/incomplete deltas
- backend-independent camera movement foundation
- GLFW desktop window and event loop
- OpenGL 3.3 Core renderer backend isolated from Scene/Core networking
- visible Region water plane and WorldModel-derived object/avatar proxies
- keyboard camera controls and resize/VSync frame loop
- optional `--render-demo` mode for local graphics verification without inventing server state
- `ogl-viewer` application shell target
- official OpenGenesisLINK Viewer application icon integrated for Windows
- Linux desktop/icon resources derived from the same supplied logo
- ordered live Core entry: discovery → authentication → Viewer bootstrap
- desktop live-Scene connection using the bootstrap endpoint and signed Scene Ticket
- authoritative initial Scene state rendered through the existing WorldModel/RenderWorld path
- periodic Scene delta polling while the graphical Viewer is running
- sequence-based reconnect that resumes from the last applied authoritative Scene sequence
- secure alpha live-launch flow that reads the password from `OGL_VIEWER_PASSWORD` rather than a process argument
- deterministic terrain-patch builder with validated sample distribution and triangle indices
- live 9×9 terrain sampling through authenticated `TERRAIN_SAMPLE` requests
- terrain patch reuse while the authoritative terrain revision is unchanged
- dynamic OpenGL terrain VAO/VBO/EBO path rendered below water/objects/avatars
- in-window graphical login form for Core URL, username, password and Region
- mouse and keyboard form navigation with masked password input
- asynchronous Core discovery/login/bootstrap so the GLFW event loop remains responsive
- visible login/connection error state inside the Viewer window
- lightweight OpenGL UI renderer with built-in bitmap text and no additional GUI toolkit dependency
- additive compatibility with both legacy root and current aggregated `session` Viewer bootstrap payloads
- exact `avatar-reconcile-v1` request/ack codec for Scene messages 152/153
- server-authoritative WASD avatar control using monotonic client movement sequences
- authoritative reconcile ACK application without skipping normal Scene event sequences
- progressive non-blocking terrain refinement from 5×5 to 9×9 to 17×17
- one-at-a-time serialized Scene I/O across sync, terrain and movement requests
- in-world runtime status overlay for Scene sequence, terrain refinement, movement/reconnect state and boundary feedback

Networking, contracts and the authoritative WorldModel remain independent from the desktop graphics backend. The current OpenGL layer is the first alpha renderer and can be replaced or supplemented later without redesigning the Scene/Core protocol stack.

Application branding is already wired into packaging. No substitute artwork is generated: the repository uses build-ready derivatives of the official Viewer logo supplied by the project.

## Build

Linux:

```bash
sudo apt-get install ninja-build libcurl4-openssl-dev nlohmann-json3-dev \
  libgl1-mesa-dev libglfw3-dev libglew-dev libglm-dev
cmake --preset release
cmake --build --preset release
ctest --preset release
```

Windows uses vcpkg dependencies from `vcpkg.json`.

## Live alpha connection

The 0.10 development build opens directly into a graphical login form when started without arguments. Enter the Core server URL, username, password and Region id, then select ENTER WORLD. The login path remains strictly ordered: release discovery, authentication, Viewer bootstrap, Scene join and authoritative Scene sync.

The command-line live path remains available for development and automated testing.

```bash
export OGL_VIEWER_PASSWORD='your-password'
./build/release/ogl-viewer \
  --connect \
  --server https://your-core.example \
  --username your-user \
  --region your-region-id
```

An optional requested spawn can be supplied with `--spawn X Y Z`. Passwords are intentionally not accepted on the command line.

This alpha path performs release discovery before authentication, logs in through Core, bootstraps the Viewer, connects to the returned Scene endpoint with the signed ticket, applies the initial authoritative Scene sync and renders the resulting Region. If the Scene connection drops, the Viewer attempts to resume from its last applied Scene sequence.

## Contract smoke check

```bash
./build/release/ogl-viewer-contract-smoke https://your-core.example
```

The utility calls both required discovery endpoints and exits non-zero if the released Viewer/Scene contracts are incompatible.

## Next milestone

The next development block builds the first Asset fetch/cache pipeline and starts consuming bootstrap Appearance/Inventory data for real avatar presentation. Client-side movement prediction and animation remain later refinements; the current movement path intentionally applies the server's authoritative reconcile result. See `docs/ROADMAP.md`.

## License

Mozilla Public License 2.0.
