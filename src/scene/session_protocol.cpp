#include "opengenesislink/viewer/scene/session_protocol.hpp"

#include <charconv>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace ogl::viewer::scene {
namespace {

void validate_component(std::string_view value, std::string_view label) {
    if (value.empty()) {
        throw std::invalid_argument(std::string(label) + " must not be empty");
    }
    if (value.find('\n') != std::string_view::npos ||
        value.find('\r') != std::string_view::npos) {
        throw std::invalid_argument(std::string(label) + " contains a line break");
    }
}

std::string required(
    const KeyValuePayload& values,
    std::string_view key) {
    const auto it = values.find(std::string(key));
    if (it == values.end() || it->second.empty()) {
        throw std::runtime_error("Scene payload is missing field: " + std::string(key));
    }
    return it->second;
}

template <typename T>
T parse_unsigned(const KeyValuePayload& values, std::string_view key) {
    const auto value = required(values, key);
    T result{};
    const auto [ptr, error] =
        std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || ptr != value.data() + value.size()) {
        throw std::runtime_error("Invalid unsigned Scene field: " + std::string(key));
    }
    return result;
}

double parse_double(const KeyValuePayload& values, std::string_view key) {
    const auto value = required(values, key);
    char* end = nullptr;
    const auto result = std::strtod(value.c_str(), &end);
    if (end == value.c_str() || *end != '\0') {
        throw std::runtime_error("Invalid floating-point Scene field: " + std::string(key));
    }
    return result;
}

void require_response(
    const Frame& frame,
    MessageType expected_type,
    std::uint32_t expected_request_id) {
    if (frame.type == MessageType::error) {
        const auto error = parse_scene_error(frame);
        throw std::runtime_error("Scene error: " + error.reason);
    }
    if (frame.type != expected_type) {
        throw std::runtime_error("Unexpected Scene response type");
    }
    if (frame.request_id != expected_request_id) {
        throw std::runtime_error("Scene response request id mismatch");
    }
}

} // namespace

KeyValuePayload parse_key_value_payload(std::string_view payload) {
    KeyValuePayload result;
    std::size_t start = 0;

    while (start < payload.size()) {
        const auto end = payload.find('\n', start);
        const auto line = payload.substr(
            start,
            end == std::string_view::npos ? payload.size() - start : end - start);

        if (!line.empty()) {
            const auto separator = line.find('=');
            if (separator == std::string_view::npos || separator == 0U) {
                throw std::runtime_error("Invalid Scene key/value payload");
            }

            auto value = line.substr(separator + 1U);
            if (!value.empty() && value.back() == '\r') {
                value.remove_suffix(1U);
            }

            result[std::string(line.substr(0, separator))] = std::string(value);
        }

        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1U;
    }

    return result;
}

std::string encode_key_value_payload(const KeyValuePayload& values) {
    std::string output;
    for (const auto& [key, value] : values) {
        validate_component(key, "Scene payload key");
        validate_component(value, "Scene payload value");
        if (key.find('=') != std::string::npos) {
            throw std::invalid_argument("Scene payload key contains '='");
        }
        output.append(key);
        output.push_back('=');
        output.append(value);
        output.push_back('\n');
    }
    return output;
}

Frame make_hello(std::uint32_t request_id) {
    return {
        .type = MessageType::hello,
        .request_id = request_id,
        .payload = {},
    };
}

HelloAck parse_hello_ack(
    const Frame& frame,
    std::uint32_t expected_request_id) {
    require_response(frame, MessageType::hello_ack, expected_request_id);
    const auto values = parse_key_value_payload(payload_as_string(frame));

    HelloAck ack;
    ack.request_id = frame.request_id;
    ack.protocol = parse_unsigned<std::uint16_t>(values, "protocol");
    ack.scene_contract = parse_unsigned<std::uint16_t>(values, "scene_contract");
    ack.server = required(values, "server");
    ack.auth = required(values, "auth");
    ack.movement = required(values, "movement");
    ack.sync = required(values, "sync");
    ack.metadata = required(values, "metadata");
    ack.capabilities = required(values, "capabilities");
    const auto runtime = values.find("object_runtime");
    if (runtime != values.end()) {
        ack.object_runtime = runtime->second;
    }

    if (ack.protocol != kOgl1ProtocolVersion) {
        throw std::runtime_error("HELLO_ACK advertises unsupported OGL1 protocol");
    }
    if (ack.scene_contract != 2U) {
        throw std::runtime_error("HELLO_ACK advertises unsupported Scene contract");
    }
    if (ack.sync != "scene-sync-v1") {
        throw std::runtime_error("HELLO_ACK advertises unsupported Scene sync contract");
    }
    if (ack.movement != "avatar-reconcile-v1") {
        throw std::runtime_error("HELLO_ACK advertises unsupported movement contract");
    }
    if (ack.capabilities != "scene-capabilities-v2") {
        throw std::runtime_error("HELLO_ACK advertises unsupported capability contract");
    }

    return ack;
}

Frame make_scene_join(
    std::uint32_t request_id,
    std::string_view region_id,
    std::string_view scene_ticket) {
    validate_component(region_id, "Region id");
    validate_component(scene_ticket, "Scene ticket");

    std::string payload;
    payload.reserve(region_id.size() + scene_ticket.size() + 16U);
    payload.append("region=");
    payload.append(region_id);
    payload.append("\nticket=");
    payload.append(scene_ticket);
    payload.push_back('\n');

    return {
        .type = MessageType::scene_join,
        .request_id = request_id,
        .payload = payload_from_string(payload),
    };
}

SceneJoinAck parse_scene_join_ack(
    const Frame& frame,
    std::uint32_t expected_request_id) {
    require_response(frame, MessageType::scene_join_ack, expected_request_id);
    const auto values = parse_key_value_payload(payload_as_string(frame));

    if (required(values, "status") != "joined") {
        throw std::runtime_error("SCENE_JOIN_ACK did not confirm joined state");
    }
    if (parse_unsigned<std::uint16_t>(values, "scene_contract") != 2U) {
        throw std::runtime_error("SCENE_JOIN_ACK Scene contract mismatch");
    }
    if (required(values, "sync") != "scene-sync-v1") {
        throw std::runtime_error("SCENE_JOIN_ACK sync contract mismatch");
    }
    if (required(values, "movement") != "avatar-reconcile-v1") {
        throw std::runtime_error("SCENE_JOIN_ACK movement contract mismatch");
    }

    SceneJoinAck ack;
    ack.request_id = frame.request_id;
    ack.region_id = required(values, "region");
    ack.user_id = required(values, "user_id");
    ack.avatar_id = parse_unsigned<std::uint64_t>(values, "avatar_id");
    ack.sequence = parse_unsigned<std::uint64_t>(values, "sequence");
    ack.terrain_revision = parse_unsigned<std::uint64_t>(values, "terrain_revision");
    ack.capabilities = required(values, "capabilities");

    const auto groups = values.find("groups");
    if (groups != values.end()) {
        ack.groups = groups->second;
    }

    ack.spawn_x = parse_double(values, "spawn_x");
    ack.spawn_y = parse_double(values, "spawn_y");
    ack.spawn_z = parse_double(values, "spawn_z");
    return ack;
}

Frame make_scene_sync_request(
    std::uint32_t request_id,
    std::uint64_t since,
    std::uint32_t max_events) {
    if (max_events < 1U || max_events > 1024U) {
        throw std::invalid_argument("Scene sync max_events must be in range 1..1024");
    }

    const auto payload =
        "since=" + std::to_string(since) +
        "\nmax_events=" + std::to_string(max_events) + "\n";
    return {
        .type = MessageType::scene_sync_request,
        .request_id = request_id,
        .payload = payload_from_string(payload),
    };
}

SceneError parse_scene_error(const Frame& frame) {
    if (frame.type != MessageType::error) {
        throw std::runtime_error("Frame is not a Scene error");
    }

    const auto values = parse_key_value_payload(payload_as_string(frame));
    SceneError error;
    error.request_id = frame.request_id;
    error.reason = required(values, "reason");
    const auto capability = values.find("capability");
    if (capability != values.end()) {
        error.capability = capability->second;
    }
    return error;
}

} // namespace ogl::viewer::scene
