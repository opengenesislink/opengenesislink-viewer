# OpenGenesisLINK Viewer roadmap

This roadmap follows the canonical Viewer handover implementation order.

| # | Area | Status |
|---:|---|---|
| 1 | Release discovery | Foundation implemented |
| 2 | Core login/session | Foundation implemented |
| 3 | Viewer bootstrap | Foundation implemented |
| 4 | OGL1 framing | Next |
| 5 | HELLO + SCENE_JOIN | Planned |
| 6 | Full Scene snapshot | Planned |
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

## Immediate next block

Implement bounded OGL1 framed TCP transport as an isolated SceneClient layer:

- frame encode/decode
- maximum frame bounds
- connection lifecycle
- HELLO negotiation
- Scene Ticket join
- deterministic disconnect cleanup
- fragmented/coalesced frame tests

No final UDP/QUIC transport is assumed.
