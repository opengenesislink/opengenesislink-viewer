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
| 7 | Region / terrain / object rendering | Terrain/water/live entities implemented; arbitrary object mesh/material visuals remain Server-contract-limited |
| 8 | Avatar rendering | Procedural humanoid fallback + Appearance height/wearable/attachment proxies implemented; final asset visuals pending format contract |
| 9 | Avatar reconciliation | First server-authoritative WASD/reconcile path implemented |
| 10 | Scene deltas / reconnect recovery | Delta polling + sequence-based reconnect implemented |
| 11 | Asset fetch / cache | Authenticated fetch + bounded in-memory cache foundation implemented |
| 12 | Appearance / wearables | Live refresh + wearable/attachment mutations + procedural visual fallback implemented; asset decode pending format contract |
| 13 | Inventory UI | Inspector + live refresh + folder/item creation implemented; unsupported mutations remain gated by Server endpoints |
| 14 | Chat / social | Local Chat + DM + friends + presence + block/mute command workflows implemented |
| 15 | Object editing / build tools | Create/delete/transform/link/text/permissions/motion/interaction/Physics command workflows implemented |
| 16 | Parcel / land UI | Region parcels, create/policy/access and terrain editing workflows implemented |
| 17 | Teleport / handoff | Native OGL teleport + transactional adjacent handoff/reserve/complete/rollback implemented |
| 18 | Groups / notifications | Groups, members, roles, invites, channel posts and notifications implemented |
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

## Graphical login UI

The 0.9 development path removes the command-line requirement for normal Viewer startup:

- default launch opens an in-window server/login form
- fields cover Core URL, username, password and Region id
- password input is masked and cleared from the form immediately after submission
- mouse selection, Tab/Shift+Tab navigation, Backspace and Enter submission are supported
- Core discovery/authentication/bootstrap runs asynchronously so the GLFW event loop remains responsive
- connection failures are shown in the Viewer instead of only on stderr
- the existing CLI connection path remains for development/automation
- the supplied official Viewer logo remains the application/window/package branding; no substitute artwork is generated
- the UI renderer is an isolated OpenGL layer and does not couple Core/Scene/WorldModel to GLFW

## Progressive world streaming and avatar reconciliation

The 0.10 development path completes the first interactive live-world control loop:

- the current aggregated Viewer bootstrap `session` object and the earlier root-field bootstrap remain accepted under the same `ogl-viewer-bootstrap-v1` contract
- terrain no longer blocks world entry while a 9×9 mesh is synchronously assembled
- refinement proceeds through 5×5, 9×9 and 17×17 levels
- samples already present in a coarser level are reused by finer levels
- only one correlated Scene request owns the TCP channel at a time
- Scene delta polling uses a non-blocking lock and yields when terrain/movement I/O owns the channel
- `AVATAR_RECONCILE` uses a monotonic client sequence and the released `avatar-reconcile-v1` payload
- WASD produces bounded normalized movement commands relative to Viewer heading
- authoritative ACK position/rotation/velocity is applied immediately to the local Avatar without advancing the normal Scene sequence
- subsequent `SCENE_SYNC` deltas remain authoritative for event history
- a zero-velocity reconcile is sent when movement input stops
- the live camera follows the authoritative local Avatar
- the in-world overlay exposes Scene sequence, terrain resolution, movement state, reconnect state and boundary/error feedback

This is server reconciliation, not final client prediction. The Viewer does not currently extrapolate unacknowledged movement or invent collision outcomes locally.

## Bootstrap content and Asset delivery foundation

The 0.11 development path now consumes the non-Scene data returned by the released Viewer bootstrap:

- Appearance is typed as revision, Avatar height, visual-parameter CSV, wearables and attachments
- Wearable references preserve slot, Inventory item id and Asset id
- Attachment references preserve attachment point, Inventory item id and Asset id
- Inventory root/folders/items and item → Asset references are retained in a rendering-independent model
- owned Asset metadata retains id, MIME type, size, permissions, next-owner permissions, creation time and the server-provided `content_hash`
- Appearance Asset dependencies are deduplicated in stable wearable/attachment order
- only Appearance dependencies are prefetched; the Viewer does not download the user's entire Asset collection at login
- Asset payloads are fetched through authenticated `GET /v1/assets/{id}`
- Base64, requested id and decoded size are validated before cache admission
- encoded HTTP responses are bounded while libcurl is receiving them, not only after Base64 decode
- the cache is bounded by both entry count and bytes and evicts least-recently-used entries
- bootstrap `content_hash` is treated as an opaque invalidation/version key
- reconnect reuses cache entries only while the new bootstrap metadata still matches
- Asset HTTP work uses a separate transport and therefore does not occupy the serialized OGL1 Scene request channel
- the in-world overlay reports Appearance revision, Inventory counts, cached/required Appearance Assets and prefetch state

The server contract currently does not define a canonical Viewer texture/mesh MIME and binary payload matrix. The Viewer therefore retains fetched bytes and metadata without pretending that arbitrary `application/octet-stream` data is renderable.

## Read-only Content Inspector

The 0.12 development path makes the already typed bootstrap data inspectable inside the running Viewer without inventing mutation semantics:

- `I` opens/closes the in-world Content Inspector
- `Tab` cycles Appearance, Inventory and Asset metadata sections
- `PageUp` / `PageDown` paginate long sections
- Appearance shows revision, Avatar height, visual-parameter CSV, wearables and attachments
- wearable rows preserve slot, Inventory item id and Asset id
- attachment rows preserve attachment point, Inventory item id and Asset id
- Inventory shows root metadata, folders, items and item → Asset references
- Asset pages show name, MIME type, byte size, id and server-provided content-hash key
- missing optional bootstrap sections are shown explicitly instead of fabricated
- the view model is independent from GLFW/OpenGL and is unit-tested for section cycling, placeholders and page clamping
- while the inspector is open, new Avatar movement input is suspended and a stop reconcile is queued when needed
- Scene synchronization, terrain refinement, Asset prefetch and world rendering continue behind the inspector
- the inspector is intentionally read-only; no Inventory or Appearance mutation endpoint is assumed

## 0.13 alpha feature completion

The 0.13 development path adds the first broad in-world workflow layer on top of the authoritative Core/Scene foundation:

- bounded Local Chat history sourced from Scene events
- Enter-activated chat/command bar with parser/unit tests
- Core PlatformClient for Appearance, Inventory, Presence/Social, Groups, Notifications, Parcels and Travel
- Appearance refresh and wearable/attachment mutations
- Inventory refresh plus released folder/item creation operations
- direct messages, friend request/accept/remove, block and mute
- group membership/roles/invites/channel posts and notification read state
- Scene-authoritative Build Tools: create/delete/transform/link/text/permissions/motion/interaction and released Physics actions
- Parcel list/create/policy/access plus authenticated terrain height modification
- native OpenGenesisLINK teleport
- adjacent Region Crossing v3 flow: destination Scene accept -> reserve -> complete, with explicit rollback and source recovery
- procedural humanoid Avatar fallback driven by authoritative Avatar height plus wearable/attachment metadata
- arbitrary object mesh/material rendering remains intentionally unresolved because the current Server visual-state contract does not publish enough canonical render data
- Hypergrid travel into OpenSimulator/OSGrid is not part of the native teleport implementation

## Immediate next block

Prepare the first installable alpha test build and close the remaining first-alpha acceptance gap:

- complete Linux x86_64 / ARM64 / Windows CI for 0.13
- package Linux and Windows test artifacts
- run a real Server ↔ Viewer login/world-entry/travel smoke test
- add Atlas destination/open handling
- define the canonical visual Asset/mesh/material payload contract with the Server
- implement Hypergrid/OpenSimulator travel as a separate interoperability milestone
- later: Economy/Marketplace and Voice provider bridge

No final UDP/QUIC transport is assumed.
