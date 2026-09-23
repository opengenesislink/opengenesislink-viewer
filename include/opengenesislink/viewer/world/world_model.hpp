#pragma once

#include "opengenesislink/viewer/scene/frame.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ogl::viewer::world {

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Transform {
    Vec3 position;
    Vec3 rotation;
    Vec3 scale{1.0, 1.0, 1.0};
};

enum class EntityKind {
    object,
    avatar
};

struct PhysicsState {
    bool physical = false;
    Vec3 velocity;
    Vec3 angular_velocity;
    double mass = 0.0;
    double restitution = 0.0;
    double friction = 0.0;
    double buoyancy = 0.0;
};

struct Entity {
    std::uint64_t id = 0;
    EntityKind kind = EntityKind::object;
    std::string name;
    Transform transform;
    std::string owner_user_id;
    std::string group_id;
    std::uint32_t owner_permissions = 0;
    std::uint32_t group_permissions = 0;
    std::uint32_t everyone_permissions = 0;
    std::uint64_t parent_entity_id = 0;
    std::uint32_t link_number = 0;
    std::string floating_text;
    PhysicsState physics;
};

struct RegionState {
    std::string region_id;
    std::uint64_t sequence = 0;
    std::size_t terrain_width = 0;
    std::size_t terrain_height = 0;
    double terrain_cell_size = 0.0;
    std::uint64_t terrain_revision = 0;
    double water_height = 0.0;
    std::unordered_map<std::uint64_t, Entity> entities;
};

struct SceneEvent {
    std::uint64_t sequence = 0;
    std::string type;
    std::uint64_t entity_id = 0;
    std::string text;
    Transform transform;
};

struct SceneDelta {
    std::string region_id;
    std::uint64_t from = 0;
    std::uint64_t latest = 0;
    std::vector<SceneEvent> events;
};

enum class SceneSyncMode {
    snapshot,
    delta
};

struct SceneSyncPayload {
    SceneSyncMode mode = SceneSyncMode::snapshot;
    std::optional<RegionState> snapshot;
    std::optional<SceneDelta> delta;
};

[[nodiscard]] SceneSyncPayload parse_scene_sync(
    const scene::Frame& frame);

struct SyncApplyResult {
    bool applied = false;
    bool requires_snapshot = false;
    std::size_t events_applied = 0;
};

class WorldModel {
public:
    [[nodiscard]] SyncApplyResult apply(
        const SceneSyncPayload& sync);

    [[nodiscard]] bool initialized() const noexcept;
    [[nodiscard]] const RegionState& region() const;
    [[nodiscard]] std::uint64_t sequence() const noexcept;

    void clear() noexcept;

private:
    [[nodiscard]] bool apply_event(const SceneEvent& event);

    RegionState region_;
    bool initialized_ = false;
};

} // namespace ogl::viewer::world
