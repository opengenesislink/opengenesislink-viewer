#include "opengenesislink/viewer/world/terrain_cache.hpp"

#include <functional>
#include <stdexcept>

namespace ogl::viewer::world {

TerrainCache::TerrainCache(scene::SceneSession& session)
    : session_(session) {}

std::size_t TerrainCache::KeyHash::operator()(const Key& key) const noexcept {
    const auto hx = std::hash<std::size_t>{}(key.x);
    const auto hy = std::hash<std::size_t>{}(key.y);
    return hx ^ (hy + 0x9e3779b9U + (hx << 6U) + (hx >> 2U));
}

double TerrainCache::sample_grid(
    const RegionState& region,
    std::size_t grid_x,
    std::size_t grid_y) {
    if (region.terrain_width == 0U ||
        region.terrain_height == 0U ||
        region.terrain_cell_size <= 0.0) {
        throw std::invalid_argument("Region has invalid terrain metadata");
    }
    if (grid_x >= region.terrain_width ||
        grid_y >= region.terrain_height) {
        throw std::out_of_range("Terrain sample grid coordinate is outside Region");
    }

    if (revision_ != region.terrain_revision) {
        samples_.clear();
        revision_ = region.terrain_revision;
    }

    const Key key{grid_x, grid_y};
    if (const auto found = samples_.find(key); found != samples_.end()) {
        return found->second;
    }

    const auto sample = session_.request_terrain_sample(
        static_cast<double>(grid_x) * region.terrain_cell_size,
        static_cast<double>(grid_y) * region.terrain_cell_size);

    if (sample.revision != region.terrain_revision) {
        samples_.clear();
        revision_ = sample.revision;
    }

    samples_[key] = sample.height;
    return sample.height;
}

void TerrainCache::invalidate() noexcept {
    samples_.clear();
    revision_ = 0;
}

std::size_t TerrainCache::cached_samples() const noexcept {
    return samples_.size();
}

std::uint64_t TerrainCache::revision() const noexcept {
    return revision_;
}

} // namespace ogl::viewer::world
