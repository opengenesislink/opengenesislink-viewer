#include "opengenesislink/viewer/input/avatar_controller.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ogl::viewer::input {

scene::AvatarReconcileRequest
make_avatar_control_request(
    std::uint64_t client_sequence,
    const world::Transform& authoritative_transform,
    const AvatarControlInput& input) {
    if (client_sequence == 0U) {
        throw std::invalid_argument(
            "Avatar control client sequence must be non-zero");
    }
    if (!std::isfinite(input.forward) ||
        !std::isfinite(input.right) ||
        !std::isfinite(input.heading_degrees) ||
        !std::isfinite(input.speed) ||
        !std::isfinite(input.command_seconds)) {
        throw std::invalid_argument(
            "Avatar control input must be finite");
    }
    if (input.speed < 0.0 || input.speed > 20.0) {
        throw std::invalid_argument(
            "Avatar control speed must be in range 0..20");
    }
    if (input.command_seconds <= 0.0 ||
        input.command_seconds > 0.5) {
        throw std::invalid_argument(
            "Avatar control command interval must be in range (0, 0.5]");
    }

    auto forward = std::clamp(input.forward, -1.0, 1.0);
    auto right = std::clamp(input.right, -1.0, 1.0);

    const auto magnitude =
        std::sqrt(forward * forward + right * right);
    if (magnitude > 1.0) {
        forward /= magnitude;
        right /= magnitude;
    }

    constexpr double pi =
        3.1415926535897932384626433832795;
    const auto heading =
        input.heading_degrees * pi / 180.0;

    const auto forward_x = std::cos(heading);
    const auto forward_y = std::sin(heading);
    const auto right_x = -forward_y;
    const auto right_y = forward_x;

    const auto velocity_x =
        (forward * forward_x + right * right_x) *
        input.speed;
    const auto velocity_y =
        (forward * forward_y + right * right_y) *
        input.speed;

    scene::AvatarReconcileRequest request;
    request.client_sequence = client_sequence;
    request.pose = {
        .x = authoritative_transform.position.x +
             velocity_x * input.command_seconds,
        .y = authoritative_transform.position.y +
             velocity_y * input.command_seconds,
        .z = authoritative_transform.position.z,
        .rx = authoritative_transform.rotation.x,
        .ry = authoritative_transform.rotation.y,
        .rz = input.heading_degrees,
    };
    request.velocity = {
        .x = velocity_x,
        .y = velocity_y,
        .z = 0.0,
    };
    return request;
}

} // namespace ogl::viewer::input
