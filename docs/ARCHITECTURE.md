# OpenGenesisLINK Viewer architecture

## Baseline

This repository targets the server contracts frozen for OpenGenesisLINK Server 1.0.0-alpha.1:

- release discovery: `ogl-release-v1`
- Core API: version 1
- Viewer bootstrap: `ogl-viewer-bootstrap-v1`
- Scene application contract: `scene-v2`
- Scene transport: OGL1 protocol 1

The server remains authoritative for identity, permissions, Region admission and simulation state.

## Layering

```text
Application shell
├── CoreClient
│   ├── ReleaseDiscovery
│   ├── AuthClient
│   ├── ViewerBootstrapClient
│   ├── Inventory / Assets
│   ├── Social / Groups
│   ├── Economy / Marketplace
│   └── Teleport / Handoff
├── SceneClient
│   ├── OGL1 framing
│   ├── SceneSession
│   ├── Snapshot / Delta replication
│   ├── Avatar reconciliation
│   └── Scene commands
├── WorldModel
│   ├── Regions
│   ├── Entities
│   ├── Terrain
│   ├── Parcels
│   └── Avatar state
├── Render
├── Input / Camera
├── UI
├── AssetCache
└── Atlas / Voice adapters
```

The network and contract layers must not depend on a rendering backend.

## Current implementation

The first foundation block implements:

1. `GET /v1` and `GET /v1/release` discovery.
2. Exact validation of released Viewer and Scene identifiers.
3. Bearer-session login through `POST /v1/auth/login`.
4. Viewer bootstrap through `POST /v1/viewer/bootstrap`.
5. Preservation of raw additive bootstrap JSON.
6. A real libcurl transport plus transport-injected unit tests.

Scene transport, rendering and UI are intentionally not implemented in this block.

## Compatibility policy

Unknown JSON fields are ignored by typed parsing. Optional capabilities are feature-detected. Breaking contract identifiers are rejected rather than guessed.

The Viewer must never infer user identity, Group membership or Region admission locally.
