#pragma once

#include "opengenesislink/viewer/world/world_model.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace ogl::viewer::world {

struct TerrainPatch {
    std::size_t columns = 0;
    std::size_t rows = 0;
    std::uint64_t revision = 0;
    std::vector<Vec3> vertices;
    std::vector<std::uint32_t> indices;
};

using TerrainHeightSampler =
    std::function<double(std::size_t grid_x, std::size_t grid_y)>;

[[nodiscard]] TerrainPatch build_terrain_patch(
    const RegionState& region,
    std::size_t requested_columns,
    std::size_t requested_rows,
    const TerrainHeightSampler& sample_height);

} // namespace ogl::viewer::world
