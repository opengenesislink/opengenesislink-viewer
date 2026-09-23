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
| 7 | Region / terrain / object rendering | WorldModel ready; render layer next |
| 8 | Avatar rendering | Planned |
| 9 | Avatar reconciliation | Planned |
| 10 | Scene deltas / reconnect recovery | Planned |
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

## Immediate next block

Build the first render-facing world layer:

- Region/terrain runtime representation
- terrain sample acquisition and cache
- entity render instances derived from WorldModel state
- primitive geometry mapping compatible with GenesisMesher output
- camera/input foundation
- snapshot recovery wiring around `requires_snapshot`
- reconnect continuation using the last authoritative Scene sequence

No final UDP/QUIC transport is assumed.
