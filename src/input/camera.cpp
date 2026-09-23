#include "opengenesislink/viewer/input/camera.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ogl::viewer::input {

void CameraController::update(
    CameraState& camera,
    const CameraInput& input,
    double delta_seconds) const {
    if (!std::isfinite(delta_seconds) || delta_seconds < 0.0) {
        throw std::invalid_argument(
            "Camera delta_seconds must be finite and non-negative");
    }

    camera.yaw_degrees += input.yaw_delta;
    while (camera.yaw_degrees >= 360.0) camera.yaw_degrees -= 360.0;
    while (camera.yaw_degrees < 0.0) camera.yaw_degrees += 360.0;

    camera.pitch_degrees = std::clamp(
        camera.pitch_degrees + input.pitch_delta,
        -89.0,
        89.0);

    constexpr double pi = 3.14159265358979323846;
    const auto yaw = camera.yaw_degrees * pi / 180.0;
    const auto speed = camera.move_speed * delta_seconds;

    const world::Vec3 forward{
        std::cos(yaw),
        std::sin(yaw),
        0.0};
    const world::Vec3 right{
        -std::sin(yaw),
        std::cos(yaw),
        0.0};

    camera.position.x +=
        (forward.x * input.forward + right.x * input.right) * speed;
    camera.position.y +=
        (forward.y * input.forward + right.y * input.right) * speed;
    camera.position.z += input.up * speed;
}

} // namespace ogl::viewer::input
