#pragma once

#include "opengenesislink/viewer/input/camera.hpp"
#include "opengenesislink/viewer/world/render_world.hpp"

#include <memory>

namespace ogl::viewer::render {

class OpenGlRenderer {
public:
    OpenGlRenderer();
    ~OpenGlRenderer();

    OpenGlRenderer(const OpenGlRenderer&) = delete;
    OpenGlRenderer& operator=(const OpenGlRenderer&) = delete;
    OpenGlRenderer(OpenGlRenderer&&) noexcept;
    OpenGlRenderer& operator=(OpenGlRenderer&&) noexcept;

    void initialize();
    void resize(int width, int height);
    void render(
        const world::RenderRegion* region,
        const input::CameraState& camera);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ogl::viewer::render
