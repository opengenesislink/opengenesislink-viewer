#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ogl::viewer::scene {

inline constexpr std::array<std::byte, 4> kOgl1Magic{
    std::byte{'O'}, std::byte{'G'}, std::byte{'L'}, std::byte{'1'}};
inline constexpr std::uint16_t kOgl1ProtocolVersion = 1;
inline constexpr std::size_t kOgl1HeaderSize = 16;
inline constexpr std::uint32_t kOgl1MaxPayloadSize = 1024U * 1024U;

enum class MessageType : std::uint16_t {
    hello = 1,
    hello_ack = 2,

    ping = 40,
    pong = 41,
    goodbye = 42,

    scene_join = 100,
    scene_join_ack = 101,
    scene_snapshot_request = 102,
    scene_snapshot = 103,

    entity_create = 110,
    entity_create_ack = 111,
    entity_update = 112,
    entity_update_ack = 113,
    entity_delete = 114,
    entity_delete_ack = 115,
    entity_permissions = 116,
    entity_permissions_ack = 117,
    entity_link = 118,
    entity_link_ack = 119,
    chat_send = 120,
    chat_event = 121,
    entity_text = 122,
    entity_text_ack = 123,
    entity_motion = 124,
    entity_motion_ack = 125,
    entity_physics = 126,
    entity_physics_ack = 127,
    entity_interact = 128,
    entity_interact_ack = 129,
    scene_events_request = 130,
    scene_events = 131,
    scene_sync_request = 132,
    scene_sync = 133,
    region_metadata_request = 134,
    region_metadata = 135,
    parcel_info_request = 136,
    parcel_info = 137,

    terrain_sample_request = 140,
    terrain_sample = 141,
    terrain_set_request = 142,
    terrain_set_ack = 143,

    avatar_move = 150,
    avatar_move_ack = 151,
    avatar_reconcile = 152,
    avatar_reconcile_ack = 153,

    error = 255
};

struct Frame {
    MessageType type{MessageType::error};
    std::uint32_t request_id{0};
    std::vector<std::byte> payload;
};

[[nodiscard]] std::vector<std::byte> encode_frame(const Frame& frame);
[[nodiscard]] Frame decode_frame(std::span<const std::byte> bytes);
[[nodiscard]] std::string payload_as_string(const Frame& frame);
[[nodiscard]] std::vector<std::byte> payload_from_string(std::string_view value);

class FrameStreamDecoder {
public:
    [[nodiscard]] std::vector<Frame> feed(std::span<const std::byte> bytes);
    void reset() noexcept;
    [[nodiscard]] std::size_t buffered_bytes() const noexcept;

private:
    std::vector<std::byte> buffer_;
};

} // namespace ogl::viewer::scene
