#include "opengenesislink/viewer/world/terrain_patch.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ogl::viewer::world {

TerrainPatch build_terrain_patch(
    const RegionState& region,
    std::size_t requested_columns,
    std::size_t requested_rows,
    const TerrainHeightSampler& sample_height) {
    if (region.terrain_width < 2U ||
        region.terrain_height < 2U ||
        region.terrain_cell_size <= 0.0) {
        throw std::invalid_argument(
            "Region terrain metadata cannot produce a terrain patch");
    }
    if (requested_columns < 2U || requested_rows < 2U) {
        throw std::invalid_argument(
            "Terrain patch requires at least 2x2 samples");
    }
    if (!sample_height) {
        throw std::invalid_argument(
            "Terrain patch requires a height sampler");
    }

    const auto columns =
        std::min(requested_columns, region.terrain_width);
    const auto rows =
        std::min(requested_rows, region.terrain_height);

    if (columns > static_cast<std::size_t>(
            std::numeric_limits<std::uint32_t>::max()) ||
        rows > static_cast<std::size_t>(
            std::numeric_limits<std::uint32_t>::max()) ||
        columns * rows > static_cast<std::size_t>(
            std::numeric_limits<std::uint32_t>::max())) {
        throw std::overflow_error(
            "Terrain patch vertex count exceeds 32-bit index range");
    }

    TerrainPatch patch;
    patch.columns = columns;
    patch.rows = rows;
    patch.revision = region.terrain_revision;
    patch.vertices.reserve(columns * rows);
    patch.indices.reserve(
        (columns - 1U) *
        (rows - 1U) *
        6U);

    const auto max_grid_x = region.terrain_width - 1U;
    const auto max_grid_y = region.terrain_height - 1U;

    for (std::size_t row = 0; row < rows; ++row) {
        const auto grid_y =
            (row * max_grid_y) / (rows - 1U);

        for (std::size_t column = 0; column < columns; ++column) {
            const auto grid_x =
                (column * max_grid_x) / (columns - 1U);
            const auto height =
                sample_height(grid_x, grid_y);

            patch.vertices.push_back({
                static_cast<double>(grid_x) *
                    region.terrain_cell_size,
                static_cast<double>(grid_y) *
                    region.terrain_cell_size,
                height,
            });
        }
    }

    for (std::size_t row = 0; row + 1U < rows; ++row) {
        for (std::size_t column = 0;
             column + 1U < columns;
             ++column) {
            const auto top_left =
                static_cast<std::uint32_t>(
                    row * columns + column);
            const auto top_right =
                static_cast<std::uint32_t>(
                    row * columns + column + 1U);
            const auto bottom_left =
                static_cast<std::uint32_t>(
                    (row + 1U) * columns + column);
            const auto bottom_right =
                static_cast<std::uint32_t>(
                    (row + 1U) * columns + column + 1U);

            patch.indices.insert(
                patch.indices.end(),
                {
                    top_left,
                    top_right,
                    bottom_right,
                    top_left,
                    bottom_right,
                    bottom_left,
                });
        }
    }

    return patch;
}

} // namespace ogl::viewer::world
