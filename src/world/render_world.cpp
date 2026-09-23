#include "opengenesislink/viewer/world/render_world.hpp"

#include <stdexcept>

namespace ogl::viewer::world {

RenderRegion RenderWorldBuilder::build(const WorldModel& model) const {
    if (!model.initialized()) {
        throw std::runtime_error("Cannot build render world from uninitialized WorldModel");
    }

    const auto& region = model.region();

    RenderRegion render;
    render.region_id = region.region_id;
    render.scene_sequence = region.sequence;
    render.terrain_revision = region.terrain_revision;
    render.terrain_width = region.terrain_width;
    render.terrain_height = region.terrain_height;
    render.terrain_cell_size = region.terrain_cell_size;
    render.water_height = region.water_height;
    render.instances.reserve(region.entities.size());

    for (const auto& [id, entity] : region.entities) {
        RenderInstance instance;
        instance.entity_id = id;
        instance.geometry =
            entity.kind == EntityKind::avatar
                ? RenderGeometry::avatar_capsule
                : RenderGeometry::box_proxy;
        instance.transform = entity.transform;
        instance.parent_entity_id = entity.parent_entity_id;
        instance.link_number = entity.link_number;
        instance.physical = entity.physics.physical;
        render.instances.emplace(id, instance);
    }

    return render;
}

} // namespace ogl::viewer::world
