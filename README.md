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

## Current status — 0.4.0-dev

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
- `ogl-viewer` application shell target
- official OpenGenesisLINK Viewer application icon integrated for Windows builds
- Linux desktop/icon installation resources from the same supplied logo

There is deliberately no rendering engine or GUI dependency in this block. Networking/contracts stay independent from the future Render layer.

The application branding is already wired into packaging. No substitute artwork is generated: the repository uses build-ready derivatives of the official Viewer logo supplied by the project.

## Build

Linux:

```bash
sudo apt-get install ninja-build libcurl4-openssl-dev nlohmann-json3-dev
cmake --preset release
cmake --build --preset release
ctest --preset release
```

Windows uses vcpkg dependencies from `vcpkg.json`.

## Contract smoke check

```bash
./build/release/ogl-viewer-contract-smoke https://your-core.example
```

The utility calls both required discovery endpoints and exits non-zero if the released Viewer/Scene contracts are incompatible.

## Next milestone

The next development block connects the WorldModel to the first render-facing Region/terrain/entity layer, including terrain samples, primitive geometry mapping, camera/input foundations and automatic snapshot recovery. See `docs/ROADMAP.md`.

## License

Mozilla Public License 2.0.
