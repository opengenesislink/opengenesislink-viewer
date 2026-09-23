#pragma once

#include "opengenesislink/viewer/scene/scene_session.hpp"
#include "opengenesislink/viewer/world/world_model.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace ogl::viewer::world {

class TerrainCache {
public:
    explicit TerrainCache(scene::SceneSession& session);

    [[nodiscard]] double sample_grid(
        const RegionState& region,
        std::size_t grid_x,
        std::size_t grid_y);

    void invalidate() noexcept;
    [[nodiscard]] std::size_t cached_samples() const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept;

private:
    struct Key {
        std::size_t x = 0;
        std::size_t y = 0;

        bool operator==(const Key&) const = default;
    };

    struct KeyHash {
        std::size_t operator()(const Key& key) const noexcept;
    };

    scene::SceneSession& session_;
    std::uint64_t revision_ = 0;
    std::unordered_map<Key, double, KeyHash> samples_;
};

} // namespace ogl::viewer::world
