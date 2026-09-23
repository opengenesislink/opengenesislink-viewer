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

## Current status — 0.1.0-dev

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

There is deliberately no rendering engine or GUI dependency in this block. Networking/contracts stay independent from the future Render layer.

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

The next development block is the OGL1 framed Scene transport: bounded frame parsing, connection lifecycle, HELLO negotiation, Scene Ticket join and deterministic disconnect cleanup. See `docs/ROADMAP.md`.

## License

Mozilla Public License 2.0.
