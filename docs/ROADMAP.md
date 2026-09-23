# OpenGenesisLINK Viewer roadmap

This roadmap follows the canonical Viewer handover implementation order.

| # | Area | Status |
|---:|---|---|
| 1 | Release discovery | Foundation implemented |
| 2 | Core login/session | Foundation implemented |
| 3 | Viewer bootstrap | Foundation implemented |
| 4 | OGL1 framing | Implemented |
| 5 | HELLO + SCENE_JOIN | Implemented over portable TCP session |
| 6 | Full Scene snapshot | Initial sync request implemented; snapshot model parser next |
| 7 | Region / terrain / object rendering | Planned |
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

## Immediate next block

Turn the initial `SCENE_SYNC` payload into the first authoritative WorldModel:

- parse `mode=snapshot` and `mode=delta`
- parse Scene sequence/revision state
- materialize Region metadata, terrain and entities from the full snapshot
- retain latest applied sequence
- apply ordered deltas
- trigger full snapshot recovery when the server falls back from stale history
- add reconnect state around the existing Scene session

No final UDP/QUIC transport is assumed.
