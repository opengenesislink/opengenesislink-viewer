#pragma once

#include "opengenesislink/viewer/scene/scene_session.hpp"
#include "opengenesislink/viewer/world/world_model.hpp"

#include <cstddef>
#include <cstdint>

namespace ogl::viewer::world {

struct SynchronizeResult {
    bool applied = false;
    bool recovered_with_snapshot = false;
    std::size_t events_applied = 0;
};

class SceneSynchronizer {
public:
    SceneSynchronizer(
        scene::SceneSession& session,
        WorldModel& model);

    [[nodiscard]] SynchronizeResult apply(
        const scene::Frame& sync_frame);

    [[nodiscard]] SynchronizeResult poll(
        std::uint32_t max_events = 256U);

private:
    scene::SceneSession& session_;
    WorldModel& model_;
};

} // namespace ogl::viewer::world
