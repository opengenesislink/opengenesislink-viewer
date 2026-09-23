#pragma once

#include "opengenesislink/viewer/world/world_model.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace ogl::viewer::world {

enum class RenderGeometry {
    box_proxy,
    avatar_capsule
};

struct RenderInstance {
    std::uint64_t entity_id = 0;
    RenderGeometry geometry = RenderGeometry::box_proxy;
    Transform transform;
    std::uint64_t parent_entity_id = 0;
    std::uint32_t link_number = 0;
    bool physical = false;
};

struct RenderRegion {
    std::string region_id;
    std::uint64_t scene_sequence = 0;
    std::uint64_t terrain_revision = 0;
    double water_height = 0.0;
    std::unordered_map<std::uint64_t, RenderInstance> instances;
};

class RenderWorldBuilder {
public:
    [[nodiscard]] RenderRegion build(const WorldModel& model) const;
};

} // namespace ogl::viewer::world
