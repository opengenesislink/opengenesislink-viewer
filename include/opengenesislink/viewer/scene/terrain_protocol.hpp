#pragma once

#include "opengenesislink/viewer/scene/frame.hpp"

#include <cstdint>

namespace ogl::viewer::scene {

struct TerrainSample {
    double x = 0.0;
    double y = 0.0;
    double height = 0.0;
    std::uint64_t revision = 0;
};

[[nodiscard]] Frame make_terrain_sample_request(
    std::uint32_t request_id,
    double x,
    double y);

[[nodiscard]] TerrainSample parse_terrain_sample(
    const Frame& frame,
    std::uint32_t expected_request_id);

} // namespace ogl::viewer::scene
