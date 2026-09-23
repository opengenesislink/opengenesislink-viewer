#pragma once

#include "opengenesislink/viewer/world/terrain_patch.hpp"
#include "opengenesislink/viewer/world/world_model.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ogl::viewer::world {

enum class RenderGeometry {
    box_proxy,
    avatar_capsule,
    avatar_humanoid
};

struct RenderInstance {
    std::uint64_t entity_id = 0;
    RenderGeometry geometry = RenderGeometry::box_proxy;
    Transform transform;
    std::uint64_t parent_entity_id = 0;
    std::uint32_t link_number = 0;
    bool physical = false;
    bool local_avatar = false;
    double avatar_height = 1.9;
    std::vector<std::string> wearable_slots;
    std::vector<std::string> attachment_points;
};

struct RenderRegion {
    std::string region_id;
    std::uint64_t scene_sequence = 0;
    std::uint64_t terrain_revision = 0;
    std::size_t terrain_width = 0;
    std::size_t terrain_height = 0;
    double terrain_cell_size = 0.0;
    double water_height = 0.0;
    std::optional<TerrainPatch> terrain_patch;
    std::unordered_map<std::uint64_t, RenderInstance> instances;
};

class RenderWorldBuilder {
public:
    [[nodiscard]] RenderRegion build(const WorldModel& model) const;
};

} // namespace ogl::viewer::world
