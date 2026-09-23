# OpenGenesisLINK Viewer roadmap

This roadmap follows the canonical Viewer handover implementation order.

| # | Area | Status |
|---:|---|---|
| 1 | Release discovery | Foundation implemented |
| 2 | Core login/session | Foundation implemented |
| 3 | Viewer bootstrap | Foundation implemented |
| 4 | OGL1 framing | Implemented |
| 5 | HELLO + SCENE_JOIN | Implemented over portable TCP session |
| 6 | Full Scene snapshot | Implemented into authoritative WorldModel |
| 7 | Region / terrain / object rendering | Live sampled terrain mesh + OpenGL proxy rendering implemented |
| 8 | Avatar rendering | Visible proxy rendering implemented; full visual avatar system planned |
| 9 | Avatar reconciliation | Planned |
| 10 | Scene deltas / reconnect recovery | Delta polling + sequence-based reconnect implemented |
| 11 | Asset fetch / cache | Planned |
| 12 | Appearance / wearables | Planned |
| 13 | Inventory UI | Planned |
| 14 | Chat / social | Planned |
| 15 | Object editing / build tools | Planned |
| 16 | Parcel / land UI | Planned |
| 17 | Teleport / handoff | Planned |
| 18 | Groups / notifications | Planned |
| 19 | Economy / Marketplace | Planned |
| 20 | Atlas bridge | Planned |
| 21 | Voice provider bridge | Planned |

## First alpha acceptance target

The first Viewer alpha is reached when a user can discover a compatible server, log in, enter a Region, see terrain and objects, see/control the Avatar, recover from reconnect, use local chat, inspect Inventory/Appearance, fetch required Assets, teleport between Regions and accept/open Atlas destinations.

## Current Scene foundation

The Viewer now has a server-compatible OGL1 protocol layer:

- exact 16-byte OGL1 header and network byte order
- 1 MiB payload bound
- bounded fragmented/coalesced stream decoding
- released Scene message ids
- HELLO / HELLO_ACK validation
- SCENE_JOIN / SCENE_JOIN_ACK correlation
- Scene error parsing
- SCENE_SYNC_REQUEST construction with the server's 1..1024 batch bound

## Live Scene connection

The Viewer now binds the OGL1 protocol to a portable blocking TCP session for Linux, Linux ARM64 and Windows:

- `host:port`, `tcp://host:port` and bracketed IPv6 endpoint parsing
- complete partial-write handling
- incremental fragmented/coalesced frame reads
- correlated request ids with unrelated-frame deferral
- live HELLO negotiation
- live Scene Ticket join
- initial `SCENE_SYNC_REQUEST since=0`
- GOODBYE plus deterministic local cleanup

## Authoritative WorldModel

The Viewer now parses `SCENE_SYNC` into a rendering-independent WorldModel:

- full `mode=snapshot` Region state
- 31-field object/avatar entity records
- transforms, ownership, permissions and linkset metadata
- Physics state and velocities
- terrain dimensions/revision and water height
- ordered `mode=delta` events
- strict Scene sequence tracking
- safe transform/avatar/text/deletion delta application
- authoritative snapshot recovery request when a delta lacks enough data to reconstruct state

Structural changes such as entity creation, link changes or permission/Physics mutations are never guessed from incomplete delta data.

## Render-facing world foundation

The Viewer now has a graphics-backend-independent layer above the authoritative WorldModel:

- RenderRegion/RenderInstance projection from current Scene state
- explicit object `box_proxy` geometry until the server exposes a visual primitive descriptor
- avatar capsule proxy instances
- authenticated `TERRAIN_SAMPLE_REQUEST` / `TERRAIN_SAMPLE` support
- terrain cache invalidated by the authoritative terrain revision
- automatic full-snapshot recovery when a delta cannot safely reconstruct state
- camera movement/yaw/pitch foundation independent of the future window/render backend

The current Scene snapshot does not expose a visual GenesisMesher PrimitiveDescriptor. The Viewer therefore does not infer sphere/cylinder/capsule visuals from physics state.

## Desktop graphics foundation

The Viewer now has its first actual desktop graphics backend:

- GLFW cross-platform application/window event loop
- OpenGL 3.3 Core renderer isolated behind `OpenGlRenderer`
- shader compile/link path and depth-tested proxy geometry
- Region water plane derived from authoritative Region dimensions/water height
- visible object and avatar proxy instances from `RenderWorld`
- keyboard camera movement and look controls
- resize-aware viewport, frame timing and VSync
- `--render-demo` local graphics mode that is explicitly separate from live server state
- official supplied Viewer application logo remains wired into Windows resources and Linux desktop packaging

The renderer still uses box geometry for object/avatar proxy drawing. This is intentional until the server exposes enough visual primitive/avatar data; the Viewer does not infer unavailable visual state.

## Live desktop world entry

The 0.7 development path now joins the existing Core, Scene and OpenGL foundations:

- strict release discovery before authentication
- Core login and Viewer bootstrap
- signed Scene Ticket connection using the returned endpoint
- authoritative initial `SCENE_SYNC` application into WorldModel
- RenderWorld rebuild from live Scene state
- periodic Scene delta polling while the desktop loop runs
- sequence-based reconnect using the last authoritative Scene sequence
- server fallback to snapshot remains authoritative when history is too old
- alpha CLI launch path keeps the password out of process arguments via `OGL_VIEWER_PASSWORD`

## Sampled terrain rendering

The 0.8 development path now renders an authoritative terrain surface:

- terrain heights come only from authenticated `TERRAIN_SAMPLE` responses
- a deterministic 9×9 grid spans the authoritative Region terrain extent
- sample coordinates are cached against the server terrain revision
- unchanged revisions reuse the existing RenderRegion terrain patch
- revision changes trigger a fresh sampled patch
- the OpenGL backend owns a separate dynamic terrain VAO/VBO/EBO path
- water, terrain, object proxies and avatar proxies remain separate render layers

The Viewer does not derive terrain heights from water level, Physics or object data. The initial 9×9 resolution is deliberately coarse to keep the alpha world-entry request count bounded.

## Immediate next block

Make live entry usable without command-line configuration and refine world interaction:

- graphical login/server form inside the Viewer window
- connection/login/bootstrap/world-entry status and errors in the application UI
- progressive terrain refinement after the initial coarse patch
- non-blocking terrain sampling so network RTT does not stall the frame loop
- first avatar-control path tied to server reconciliation
- preserve the supplied Viewer logo throughout login/window/package branding

No final UDP/QUIC transport is assumed.
