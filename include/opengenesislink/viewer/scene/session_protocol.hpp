#pragma once

#include "opengenesislink/viewer/scene/frame.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace ogl::viewer::scene {

using KeyValuePayload = std::map<std::string, std::string>;

[[nodiscard]] KeyValuePayload parse_key_value_payload(std::string_view payload);
[[nodiscard]] std::string encode_key_value_payload(const KeyValuePayload& values);

struct HelloAck {
    std::uint32_t request_id = 0;
    std::uint16_t protocol = 0;
    std::uint16_t scene_contract = 0;
    std::string server;
    std::string auth;
    std::string movement;
    std::string sync;
    std::string metadata;
    std::string capabilities;
    std::string object_runtime;
};

struct SceneJoinAck {
    std::uint32_t request_id = 0;
    std::string region_id;
    std::string user_id;
    std::uint64_t avatar_id = 0;
    std::uint64_t sequence = 0;
    std::uint64_t terrain_revision = 0;
    std::string capabilities;
    std::string groups;
    double spawn_x = 0.0;
    double spawn_y = 0.0;
    double spawn_z = 0.0;
};

struct SceneError {
    std::uint32_t request_id = 0;
    std::string reason;
    std::string capability;
};

[[nodiscard]] Frame make_hello(std::uint32_t request_id);
[[nodiscard]] HelloAck parse_hello_ack(
    const Frame& frame,
    std::uint32_t expected_request_id);

[[nodiscard]] Frame make_scene_join(
    std::uint32_t request_id,
    std::string_view region_id,
    std::string_view scene_ticket);
[[nodiscard]] SceneJoinAck parse_scene_join_ack(
    const Frame& frame,
    std::uint32_t expected_request_id);

[[nodiscard]] Frame make_scene_sync_request(
    std::uint32_t request_id,
    std::uint64_t since,
    std::uint32_t max_events = 256);

[[nodiscard]] SceneError parse_scene_error(const Frame& frame);

} // namespace ogl::viewer::scene
