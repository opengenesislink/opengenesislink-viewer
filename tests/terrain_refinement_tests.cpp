#include "opengenesislink/viewer/world/terrain_refinement.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

using namespace ogl::viewer::world;

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
    region.terrain_width = 17U;
    region.terrain_height = 17U;
    region.terrain_cell_size = 1.0;
    region.terrain_revision = 9U;
    region.water_height = 20.0;
    return region;
}

void fill_current_level(TerrainRefinement& refinement) {
    while (const auto point = refinement.next_sample()) {
        refinement.submit(
            *point,
            static_cast<double>(
                point->x + point->y * 10U),
            refinement.revision());
    }
}

void test_progressive_levels_reuse_samples() {
    TerrainRefinement refinement;
    refinement.reset(
        region_fixture(),
        {5U, 9U, 17U});

    require(refinement.active(),
            "terrain refinement must start active");
    require(refinement.target_resolution() == 5U,
            "terrain refinement first resolution mismatch");

    fill_current_level(refinement);
    require(refinement.cached_samples() == 25U,
            "5x5 refinement sample count mismatch");

    auto patch = refinement.take_completed_patch();
    require(patch.has_value(),
            "5x5 terrain patch was not completed");
    require(patch->columns == 5U &&
            patch->rows == 5U,
            "5x5 terrain patch dimensions mismatch");
    require(refinement.target_resolution() == 9U,
            "terrain refinement did not advance to 9x9");

    fill_current_level(refinement);
    require(refinement.cached_samples() == 81U,
            "9x9 refinement should reuse 5x5 samples");

    patch = refinement.take_completed_patch();
    require(patch.has_value() &&
            patch->columns == 9U,
            "9x9 terrain patch was not completed");
    require(refinement.target_resolution() == 17U,
            "terrain refinement did not advance to 17x17");

    fill_current_level(refinement);
    require(refinement.cached_samples() == 289U,
            "17x17 refinement should reuse prior levels");

    patch = refinement.take_completed_patch();
    require(patch.has_value() &&
            patch->columns == 17U,
            "17x17 terrain patch was not completed");
    require(!refinement.active(),
            "terrain refinement should complete after final level");
    require(!refinement.next_sample().has_value(),
            "completed terrain refinement returned another sample");
}

void test_revision_guard() {
    TerrainRefinement refinement;
    refinement.reset(
        region_fixture(),
        {5U});

    const auto point = refinement.next_sample();
    require(point.has_value(),
            "terrain refinement did not provide first sample");

    require_throws(
        [&] {
            refinement.submit(
                *point,
                21.0,
                10U);
        },
        "terrain sample with changed revision must fail");
}

void test_level_clamping_and_validation() {
    auto region = region_fixture();
    region.terrain_width = 6U;
    region.terrain_height = 4U;

    TerrainRefinement refinement;
    refinement.reset(
        region,
        {5U, 9U, 17U});

    require(refinement.target_resolution() == 4U,
            "terrain refinement levels were not clamped to Region grid");

    require_throws(
        [&] {
            TerrainRefinement invalid;
            invalid.reset(region, {});
        },
        "empty terrain refinement levels must fail");
}

} // namespace

int main() {
    try {
        test_progressive_levels_reuse_samples();
        test_revision_guard();
        test_level_clamping_and_validation();
        std::cout
            << "OpenGenesisLINK Viewer terrain refinement tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr
            << "Terrain refinement test failure: "
            << ex.what()
            << "\n";
        return EXIT_FAILURE;
    }
}
