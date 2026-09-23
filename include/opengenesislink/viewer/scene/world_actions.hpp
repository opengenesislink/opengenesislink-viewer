#pragma once

#include "opengenesislink/viewer/scene/frame.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ogl::viewer::scene {

struct SceneTransform {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double rx = 0.0;
    double ry = 0.0;
    double rz = 0.0;
    double sx = 1.0;
    double sy = 1.0;
    double sz = 1.0;
};

struct SceneCommandAck {
    std::string status;
    std::uint64_t entity_id = 0;
    std::uint64_t sequence = 0;
    std::uint64_t revision = 0;
};

struct ObjectCreateRequest {
    std::string name = "Object";
    SceneTransform transform;
    bool physical = false;
    std::string group_id;
    std::uint32_t group_permissions = 0;
    std::uint32_t everyone_permissions = 0;
};

struct ObjectPermissionsRequest {
    std::uint64_t entity_id = 0;
    std::string group_id;
    std::uint32_t group_permissions = 0;
    std::uint32_t everyone_permissions = 0;
};

struct ObjectMotionRequest {
    std::uint64_t entity_id = 0;
    double vx = 0.0;
    double vy = 0.0;
    double vz = 0.0;
    double avx = 0.0;
    double avy = 0.0;
    double avz = 0.0;
};

struct ParcelRecord {
    std::string id;
    std::string name;
    std::string owner_user_id;
    std::string group_id;
    std::uint16_t x1 = 0;
    std::uint16_t y1 = 0;
    std::uint16_t x2 = 0;
    std::uint16_t y2 = 0;
    bool public_entry = false;
    bool public_build = false;
    bool group_build = false;
    bool group_terraform = false;
};

struct ParcelInfoResult {
    std::string mode;
    std::vector<ParcelRecord> parcels;
};

[[nodiscard]] Frame make_chat_send(
    std::uint32_t request_id,
    std::string_view text);

[[nodiscard]] Frame make_object_create(
    std::uint32_t request_id,
    const ObjectCreateRequest& request);

[[nodiscard]] Frame make_object_update(
    std::uint32_t request_id,
    std::uint64_t entity_id,
    const SceneTransform& transform);

[[nodiscard]] Frame make_object_delete(
    std::uint32_t request_id,
    std::uint64_t entity_id);

[[nodiscard]] Frame make_object_permissions(
    std::uint32_t request_id,
    const ObjectPermissionsRequest& request);

[[nodiscard]] Frame make_object_link(
    std::uint32_t request_id,
    std::uint64_t root_id,
    std::uint64_t child_id,
    bool unlink);

[[nodiscard]] Frame make_object_text(
    std::uint32_t request_id,
    std::uint64_t entity_id,
    std::string_view text);

[[nodiscard]] Frame make_object_motion(
    std::uint32_t request_id,
    const ObjectMotionRequest& request);

[[nodiscard]] Frame make_object_physics(
    std::uint32_t request_id,
    std::uint64_t entity_id,
    std::string_view action,
    std::string_view additional_fields = {});

[[nodiscard]] Frame make_object_interact(
    std::uint32_t request_id,
    std::uint64_t entity_id,
    std::string_view phase);

[[nodiscard]] Frame make_parcel_info_request(
    std::uint32_t request_id,
    std::optional<double> x = std::nullopt,
    std::optional<double> y = std::nullopt);

[[nodiscard]] Frame make_terrain_set_request(
    std::uint32_t request_id,
    std::size_t grid_x,
    std::size_t grid_y,
    double height);

[[nodiscard]] SceneCommandAck parse_scene_command_ack(
    const Frame& frame,
    MessageType expected_type,
    std::uint32_t expected_request_id);

[[nodiscard]] ParcelInfoResult parse_parcel_info(
    const Frame& frame,
    std::uint32_t expected_request_id);

} // namespace ogl::viewer::scene
