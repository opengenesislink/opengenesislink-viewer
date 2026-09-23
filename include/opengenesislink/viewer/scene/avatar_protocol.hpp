#pragma once

#include "opengenesislink/viewer/scene/frame.hpp"

#include <cstdint>
#include <string>

namespace ogl::viewer::scene {

struct AvatarPose {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double rx = 0.0;
    double ry = 0.0;
    double rz = 0.0;
};

struct AvatarVelocity {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct AvatarReconcileRequest {
    std::uint64_t client_sequence = 0;
    AvatarPose pose;
    AvatarVelocity velocity;
};

struct AvatarReconcileAck {
    std::uint64_t client_sequence = 0;
    std::uint64_t server_sequence = 0;
    std::uint64_t tick = 0;
    std::string boundary;
    AvatarPose pose;
    AvatarVelocity velocity;
};

[[nodiscard]] Frame make_avatar_reconcile(
    std::uint32_t request_id,
    const AvatarReconcileRequest& request);

[[nodiscard]] AvatarReconcileAck parse_avatar_reconcile_ack(
    const Frame& frame,
    std::uint32_t expected_request_id);

} // namespace ogl::viewer::scene
