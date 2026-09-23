#include "opengenesislink/viewer/world/terrain_patch.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

using ogl::viewer::world::RegionState;
using ogl::viewer::world::build_terrain_patch;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <typename Function>
void require_throws(Function&& function, const char* message) {
    bool threw = false;
    try {
        function();
    } catch (const std::exception&) {
        threw = true;
    }
    require(threw, message);
}

RegionState region_fixture() {
    RegionState region;
    region.region_id = "region-1";
    region.terrain_width = 5U;
    region.terrain_height = 5U;
    region.terrain_cell_size = 2.0;
    region.terrain_revision = 7U;
    region.water_height = 20.0;
    return region;
}

void test_patch_sampling_and_indices() {
    auto region = region_fixture();
    std::size_t samples = 0U;

    const auto patch = build_terrain_patch(
        region,
        3U,
        3U,
        [&samples](
            std::size_t grid_x,
            std::size_t grid_y) {
            ++samples;
            return static_cast<double>(
                grid_x + grid_y * 10U);
        });

    require(patch.columns == 3U, "terrain patch column count mismatch");
    require(patch.rows == 3U, "terrain patch row count mismatch");
    require(patch.revision == 7U, "terrain patch revision mismatch");
    require(patch.vertices.size() == 9U, "terrain patch vertex count mismatch");
    require(patch.indices.size() == 24U, "terrain patch index count mismatch");
    require(samples == 9U, "terrain patch sampler call count mismatch");

    const auto& first = patch.vertices.front();
    require(first.x == 0.0 && first.y == 0.0 && first.z == 0.0,
            "terrain patch first vertex mismatch");

    const auto& center = patch.vertices[4U];
    require(center.x == 4.0 && center.y == 4.0 && center.z == 22.0,
            "terrain patch center vertex mismatch");

    const auto& last = patch.vertices.back();
    require(last.x == 8.0 && last.y == 8.0 && last.z == 44.0,
            "terrain patch last vertex mismatch");

    require(
        patch.indices[0U] == 0U &&
        patch.indices[1U] == 1U &&
        patch.indices[2U] == 4U &&
        patch.indices[3U] == 0U &&
        patch.indices[4U] == 4U &&
        patch.indices[5U] == 3U,
        "terrain patch first cell winding mismatch");
}

void test_requested_resolution_clamps_to_region_grid() {
    auto region = region_fixture();

    const auto patch = build_terrain_patch(
        region,
        20U,
        20U,
        [](
            std::size_t grid_x,
            std::size_t grid_y) {
            return static_cast<double>(grid_x + grid_y);
        });

    require(patch.columns == 5U && patch.rows == 5U,
            "terrain patch did not clamp to Region grid");
    require(patch.vertices.size() == 25U,
            "clamped terrain patch vertex count mismatch");
    require(patch.indices.size() == 96U,
            "clamped terrain patch index count mismatch");
}

void test_patch_validation() {
    auto region = region_fixture();

    require_throws([&] {
        (void)build_terrain_patch(
            region,
            1U,
            3U,
            [](std::size_t, std::size_t) { return 0.0; });
    }, "terrain patch with one column must fail");

    require_throws([&] {
        auto invalid = region;
        invalid.terrain_cell_size = 0.0;
        (void)build_terrain_patch(
            invalid,
            3U,
            3U,
            [](std::size_t, std::size_t) { return 0.0; });
    }, "terrain patch with invalid cell size must fail");

    require_throws([&] {
        const ogl::viewer::world::TerrainHeightSampler no_sampler;
        (void)build_terrain_patch(
            region,
            3U,
            3U,
            no_sampler);
    }, "terrain patch without sampler must fail");
}

} // namespace

int main() {
    try {
        test_patch_sampling_and_indices();
        test_requested_resolution_clamps_to_region_grid();
        test_patch_validation();
        std::cout << "OpenGenesisLINK Viewer terrain patch tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr << "Terrain patch test failure: "
                  << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}
