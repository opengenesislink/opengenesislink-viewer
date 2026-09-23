#include "opengenesislink/viewer/world/terrain_refinement.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace ogl::viewer::world {
namespace {

std::size_t coordinate_for(
    std::size_t index,
    std::size_t count,
    std::size_t extent) {
    return (index * (extent - 1U)) /
           (count - 1U);
}

} // namespace

void TerrainRefinement::reset(
    const RegionState& region,
    std::vector<std::size_t> levels) {
    if (region.terrain_width < 2U ||
        region.terrain_height < 2U ||
        region.terrain_cell_size <= 0.0) {
        throw std::invalid_argument(
            "Region terrain metadata cannot be refined");
    }
    if (levels.empty()) {
        throw std::invalid_argument(
            "Terrain refinement requires at least one level");
    }

    for (auto& level : levels) {
        if (level < 2U) {
            throw std::invalid_argument(
                "Terrain refinement level must be at least 2");
        }
        level = std::min({
            level,
            region.terrain_width,
            region.terrain_height,
        });
    }

    std::sort(levels.begin(), levels.end());
    levels.erase(
        std::unique(levels.begin(), levels.end()),
        levels.end());

    metadata_ = {};
    metadata_.region_id = region.region_id;
    metadata_.terrain_width = region.terrain_width;
    metadata_.terrain_height = region.terrain_height;
    metadata_.terrain_cell_size = region.terrain_cell_size;
    metadata_.terrain_revision = region.terrain_revision;
    metadata_.water_height = region.water_height;

    levels_ = std::move(levels);
    level_index_ = 0U;
    samples_.clear();
}

bool TerrainRefinement::active() const noexcept {
    return level_index_ < levels_.size();
}

std::uint64_t TerrainRefinement::revision() const noexcept {
    return metadata_.terrain_revision;
}

std::size_t TerrainRefinement::target_resolution() const noexcept {
    return active() ? levels_[level_index_] : 0U;
}

std::size_t TerrainRefinement::cached_samples() const noexcept {
    return samples_.size();
}

std::vector<TerrainGridPoint>
TerrainRefinement::required_points(
    std::size_t resolution) const {
    std::vector<TerrainGridPoint> points;
    points.reserve(resolution * resolution);

    for (std::size_t row = 0U;
         row < resolution;
         ++row) {
        const auto grid_y =
            coordinate_for(
                row,
                resolution,
                metadata_.terrain_height);
        for (std::size_t column = 0U;
             column < resolution;
             ++column) {
            const auto grid_x =
                coordinate_for(
                    column,
                    resolution,
                    metadata_.terrain_width);
            points.push_back({
                .x = grid_x,
                .y = grid_y,
            });
        }
    }
    return points;
}

std::optional<TerrainGridPoint>
TerrainRefinement::next_sample() const {
    if (!active()) {
        return std::nullopt;
    }

    for (const auto& point :
         required_points(levels_[level_index_])) {
        if (!samples_.contains({point.x, point.y})) {
            return point;
        }
    }
    return std::nullopt;
}

void TerrainRefinement::submit(
    const TerrainGridPoint& point,
    double height,
    std::uint64_t revision) {
    if (!active()) {
        throw std::runtime_error(
            "Terrain refinement is already complete");
    }
    if (revision != metadata_.terrain_revision) {
        throw std::runtime_error(
            "Terrain sample revision changed during refinement");
    }
    if (!std::isfinite(height)) {
        throw std::invalid_argument(
            "Terrain sample height must be finite");
    }
    if (point.x >= metadata_.terrain_width ||
        point.y >= metadata_.terrain_height) {
        throw std::out_of_range(
            "Terrain sample point is outside Region grid");
    }

    samples_[{point.x, point.y}] = height;
}

std::optional<TerrainPatch>
TerrainRefinement::take_completed_patch() {
    if (!active()) {
        return std::nullopt;
    }

    const auto resolution = levels_[level_index_];
    const auto points = required_points(resolution);
    for (const auto& point : points) {
        if (!samples_.contains({point.x, point.y})) {
            return std::nullopt;
        }
    }

    auto patch = build_terrain_patch(
        metadata_,
        resolution,
        resolution,
        [this](
            std::size_t grid_x,
            std::size_t grid_y) {
            const auto found =
                samples_.find({grid_x, grid_y});
            if (found == samples_.end()) {
                throw std::runtime_error(
                    "Terrain refinement cache is incomplete");
            }
            return found->second;
        });

    ++level_index_;
    return patch;
}

} // namespace ogl::viewer::world
