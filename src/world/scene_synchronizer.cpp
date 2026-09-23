#include "opengenesislink/viewer/world/scene_synchronizer.hpp"

#include <stdexcept>

namespace ogl::viewer::world {

SceneSynchronizer::SceneSynchronizer(
    scene::SceneSession& session,
    WorldModel& model)
    : session_(session),
      model_(model) {}

SynchronizeResult SceneSynchronizer::apply(
    const scene::Frame& sync_frame) {
    const auto parsed = parse_scene_sync(sync_frame);
    const auto applied = model_.apply(parsed);

    if (!applied.requires_snapshot) {
        return {
            .applied = applied.applied,
            .recovered_with_snapshot = false,
            .events_applied = applied.events_applied,
        };
    }

    const auto recovery_frame = session_.request_sync(0U, 256U);
    const auto recovery = parse_scene_sync(recovery_frame);
    if (recovery.mode != SceneSyncMode::snapshot) {
        throw std::runtime_error(
            "Authoritative Scene recovery did not return a snapshot");
    }

    const auto recovered = model_.apply(recovery);
    if (!recovered.applied || recovered.requires_snapshot) {
        throw std::runtime_error(
            "Authoritative Scene snapshot recovery could not be applied");
    }

    return {
        .applied = true,
        .recovered_with_snapshot = true,
        .events_applied = applied.events_applied,
    };
}

SynchronizeResult SceneSynchronizer::poll(
    std::uint32_t max_events) {
    if (!model_.initialized()) {
        return apply(session_.request_sync(0U, max_events));
    }
    return apply(session_.request_sync(model_.sequence(), max_events));
}

} // namespace ogl::viewer::world
