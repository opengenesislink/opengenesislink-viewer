#include "opengenesislink/viewer/scene/terrain_protocol.hpp"

#include "opengenesislink/viewer/scene/session_protocol.hpp"

#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string>

namespace ogl::viewer::scene {
namespace {

double required_double(const KeyValuePayload& values, const char* key) {
    const auto it = values.find(key);
    if (it == values.end() || it->second.empty()) {
        throw std::runtime_error(std::string("Terrain sample is missing field: ") + key);
    }

    double result = 0.0;
    const auto [ptr, error] =
        std::from_chars(it->second.data(), it->second.data() + it->second.size(), result);
    if (error != std::errc{} || ptr != it->second.data() + it->second.size()) {
        throw std::runtime_error(std::string("Invalid terrain sample field: ") + key);
    }
    return result;
}

std::uint64_t required_u64(const KeyValuePayload& values, const char* key) {
    const auto it = values.find(key);
    if (it == values.end() || it->second.empty()) {
        throw std::runtime_error(std::string("Terrain sample is missing field: ") + key);
    }

    std::uint64_t result = 0;
    const auto [ptr, error] =
        std::from_chars(it->second.data(), it->second.data() + it->second.size(), result);
    if (error != std::errc{} || ptr != it->second.data() + it->second.size()) {
        throw std::runtime_error(std::string("Invalid terrain sample field: ") + key);
    }
    return result;
}

std::string encode_number(double value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("Terrain sample coordinate must be finite");
    }

    char buffer[64]{};
    const auto [ptr, error] = std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (error != std::errc{}) {
        throw std::runtime_error("Failed to encode terrain sample coordinate");
    }
    return std::string(buffer, ptr);
}

} // namespace

Frame make_terrain_sample_request(
    std::uint32_t request_id,
    double x,
    double y) {
    const auto payload =
        "x=" + encode_number(x) + "\ny=" + encode_number(y) + "\n";

    return {
        .type = MessageType::terrain_sample_request,
        .request_id = request_id,
        .payload = payload_from_string(payload),
    };
}

TerrainSample parse_terrain_sample(
    const Frame& frame,
    std::uint32_t expected_request_id) {
    if (frame.type == MessageType::error) {
        const auto error = parse_scene_error(frame);
        throw std::runtime_error("Terrain sample failed: " + error.reason);
    }
    if (frame.type != MessageType::terrain_sample) {
        throw std::runtime_error("Frame is not TERRAIN_SAMPLE");
    }
    if (frame.request_id != expected_request_id) {
        throw std::runtime_error("Terrain sample request id mismatch");
    }

    const auto values = parse_key_value_payload(payload_as_string(frame));
    return {
        .x = required_double(values, "x"),
        .y = required_double(values, "y"),
        .height = required_double(values, "height"),
        .revision = required_u64(values, "revision"),
    };
}

} // namespace ogl::viewer::scene
