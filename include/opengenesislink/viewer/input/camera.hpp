#pragma once

#include "opengenesislink/viewer/world/world_model.hpp"

namespace ogl::viewer::input {

struct CameraInput {
    double forward = 0.0;
    double right = 0.0;
    double up = 0.0;
    double yaw_delta = 0.0;
    double pitch_delta = 0.0;
};

struct CameraState {
    world::Vec3 position{128.0, 128.0, 30.0};
    double yaw_degrees = 0.0;
    double pitch_degrees = -15.0;
    double move_speed = 8.0;
};

class CameraController {
public:
    void update(
        CameraState& camera,
        const CameraInput& input,
        double delta_seconds) const;
};

} // namespace ogl::viewer::input
