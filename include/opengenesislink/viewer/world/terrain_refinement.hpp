#pragma once

#include "opengenesislink/viewer/world/terrain_patch.hpp"
#include "opengenesislink/viewer/world/world_model.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace ogl::viewer::world {

struct TerrainGridPoint {
    std::size_t x = 0;
    std::size_t y = 0;

    bool operator==(const TerrainGridPoint&) const = default;
};

class TerrainRefinement {
public:
    void reset(
        const RegionState& region,
        std::vector<std::size_t> levels = {5U, 9U, 17U});

    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept;
    [[nodiscard]] std::size_t target_resolution() const noexcept;
    [[nodiscard]] std::size_t cached_samples() const noexcept;

    [[nodiscard]] std::optional<TerrainGridPoint>
    next_sample() const;

    void submit(
        const TerrainGridPoint& point,
        double height,
        std::uint64_t revision);

    [[nodiscard]] std::optional<TerrainPatch>
    take_completed_patch();

private:
    [[nodiscard]] std::vector<TerrainGridPoint>
    required_points(std::size_t resolution) const;

    RegionState metadata_;
    std::vector<std::size_t> levels_;
    std::size_t level_index_ = 0U;
    std::map<std::pair<std::size_t, std::size_t>, double> samples_;
};

} // namespace ogl::viewer::world
