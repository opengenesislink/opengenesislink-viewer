#pragma once

#include "opengenesislink/viewer/scene/avatar_protocol.hpp"
#include "opengenesislink/viewer/world/world_model.hpp"

#include <cstdint>

namespace ogl::viewer::input {

struct AvatarControlInput {
    double forward = 0.0;
    double right = 0.0;
    double heading_degrees = 0.0;
    double speed = 4.0;
    double command_seconds = 0.1;
};

[[nodiscard]] scene::AvatarReconcileRequest
make_avatar_control_request(
    std::uint64_t client_sequence,
    const world::Transform& authoritative_transform,
    const AvatarControlInput& input);

} // namespace ogl::viewer::input
