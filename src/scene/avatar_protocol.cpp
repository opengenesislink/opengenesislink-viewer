#include "opengenesislink/viewer/scene/avatar_protocol.hpp"

#include "opengenesislink/viewer/scene/session_protocol.hpp"

#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ogl::viewer::scene {
namespace {

std::string required(
    const KeyValuePayload& values,
    std::string_view key) {
    const auto found = values.find(std::string(key));
    if (found == values.end() || found->second.empty()) {
        throw std::runtime_error(
            "Avatar reconcile payload is missing field: " +
            std::string(key));
    }
    return found->second;
}

std::uint64_t required_u64(
    const KeyValuePayload& values,
    std::string_view key) {
    const auto value = required(values, key);
    std::uint64_t result = 0;
    const auto [ptr, error] =
        std::from_chars(
            value.data(),
            value.data() + value.size(),
            result);
    if (error != std::errc{} ||
        ptr != value.data() + value.size()) {
        throw std::runtime_error(
            "Invalid avatar reconcile integer field: " +
            std::string(key));
    }
    return result;
}

double required_double(
    const KeyValuePayload& values,
    std::string_view key) {
    const auto value = required(values, key);
    double result = 0.0;
    const auto [ptr, error] =
        std::from_chars(
            value.data(),
            value.data() + value.size(),
            result);
    if (error != std::errc{} ||
        ptr != value.data() + value.size() ||
        !std::isfinite(result)) {
        throw std::runtime_error(
            "Invalid avatar reconcile floating-point field: " +
            std::string(key));
    }
    return result;
}

std::string encode_double(double value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(
            "Avatar reconcile value must be finite");
    }

    char buffer[64]{};
    const auto [ptr, error] =
        std::to_chars(
            buffer,
            buffer + sizeof(buffer),
            value);
    if (error != std::errc{}) {
        throw std::runtime_error(
            "Failed to encode avatar reconcile value");
    }
    return std::string(buffer, ptr);
}

} // namespace

Frame make_avatar_reconcile(
    std::uint32_t request_id,
    const AvatarReconcileRequest& request) {
    if (request.client_sequence == 0U) {
        throw std::invalid_argument(
            "Avatar reconcile client sequence must be non-zero");
    }

    std::string payload;
    payload.reserve(192U);
    payload +=
        "client_sequence=" +
        std::to_string(request.client_sequence) +
        "\nx=" + encode_double(request.pose.x) +
        "\ny=" + encode_double(request.pose.y) +
        "\nz=" + encode_double(request.pose.z) +
        "\nrx=" + encode_double(request.pose.rx) +
        "\nry=" + encode_double(request.pose.ry) +
        "\nrz=" + encode_double(request.pose.rz) +
        "\nvx=" + encode_double(request.velocity.x) +
        "\nvy=" + encode_double(request.velocity.y) +
        "\nvz=" + encode_double(request.velocity.z) +
        "\n";

    return {
        .type = MessageType::avatar_reconcile,
        .request_id = request_id,
        .payload = payload_from_string(payload),
    };
}

AvatarReconcileAck parse_avatar_reconcile_ack(
    const Frame& frame,
    std::uint32_t expected_request_id) {
    if (frame.type == MessageType::error) {
        const auto error = parse_scene_error(frame);
        throw std::runtime_error(
            "Avatar reconcile failed: " + error.reason);
    }
    if (frame.type != MessageType::avatar_reconcile_ack) {
        throw std::runtime_error(
            "Frame is not AVATAR_RECONCILE_ACK");
    }
    if (frame.request_id != expected_request_id) {
        throw std::runtime_error(
            "Avatar reconcile request id mismatch");
    }

    const auto values =
        parse_key_value_payload(
            payload_as_string(frame));
    if (required(values, "status") != "reconciled") {
        throw std::runtime_error(
            "Avatar reconcile response did not confirm reconciled state");
    }

    AvatarReconcileAck ack;
    ack.client_sequence =
        required_u64(values, "client_sequence");
    ack.server_sequence =
        required_u64(values, "server_sequence");
    ack.tick = required_u64(values, "tick");

    if (const auto boundary = values.find("boundary");
        boundary != values.end()) {
        ack.boundary = boundary->second;
    }

    ack.pose = {
        .x = required_double(values, "x"),
        .y = required_double(values, "y"),
        .z = required_double(values, "z"),
        .rx = required_double(values, "rx"),
        .ry = required_double(values, "ry"),
        .rz = required_double(values, "rz"),
    };
    ack.velocity = {
        .x = required_double(values, "vx"),
        .y = required_double(values, "vy"),
        .z = required_double(values, "vz"),
    };
    return ack;
}

} // namespace ogl::viewer::scene
