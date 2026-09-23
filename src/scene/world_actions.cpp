#include "opengenesislink/viewer/scene/world_actions.hpp"

#include "opengenesislink/viewer/scene/session_protocol.hpp"

#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ogl::viewer::scene {
namespace {

void require_id(std::uint64_t id, std::string_view label) {
    if (id == 0U) {
        throw std::invalid_argument(
            std::string(label) + " must be non-zero");
    }
}

void safe_line(std::string_view value, std::string_view label) {
    if (value.find('\n') != std::string_view::npos ||
        value.find('\r') != std::string_view::npos) {
        throw std::invalid_argument(
            std::string(label) + " contains a line break");
    }
}

std::string number(double value, std::string_view label) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(
            std::string(label) + " must be finite");
    }
    char buffer[64]{};
    const auto [ptr, error] =
        std::to_chars(
            buffer,
            buffer + sizeof(buffer),
            value);
    if (error != std::errc{}) {
        throw std::runtime_error(
            "Failed to encode Scene floating-point value");
    }
    return std::string(buffer, ptr);
}

void append_transform(
    std::string& payload,
    const SceneTransform& transform) {
    payload +=
        "x=" + number(transform.x, "x") +
        "\ny=" + number(transform.y, "y") +
        "\nz=" + number(transform.z, "z") +
        "\nrx=" + number(transform.rx, "rx") +
        "\nry=" + number(transform.ry, "ry") +
        "\nrz=" + number(transform.rz, "rz") +
        "\nsx=" + number(transform.sx, "sx") +
        "\nsy=" + number(transform.sy, "sy") +
        "\nsz=" + number(transform.sz, "sz") +
        "\n";
}

std::vector<std::string_view> split(
    std::string_view value,
    char separator) {
    std::vector<std::string_view> fields;
    std::size_t start = 0U;
    while (true) {
        const auto end = value.find(separator, start);
        fields.push_back(value.substr(
            start,
            end == std::string_view::npos
                ? std::string_view::npos
                : end - start));
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1U;
    }
    return fields;
}

template <typename T>
T parse_unsigned(
    std::string_view value,
    std::string_view label) {
    T result{};
    const auto [ptr, error] =
        std::from_chars(
            value.data(),
            value.data() + value.size(),
            result);
    if (error != std::errc{} ||
        ptr != value.data() + value.size()) {
        throw std::runtime_error(
            "Invalid " + std::string(label));
    }
    return result;
}

bool parse_bool01(
    std::string_view value,
    std::string_view label) {
    if (value == "0") return false;
    if (value == "1") return true;
    throw std::runtime_error(
        "Invalid " + std::string(label));
}

} // namespace

Frame make_chat_send(
    std::uint32_t request_id,
    std::string_view text) {
    safe_line(text, "chat text");
    if (text.empty() || text.size() > 512U) {
        throw std::invalid_argument(
            "Chat text must contain 1..512 bytes");
    }
    return {
        .type = MessageType::chat_send,
        .request_id = request_id,
        .payload = payload_from_string(
            "text=" + std::string(text) + "\n"),
    };
}

Frame make_object_create(
    std::uint32_t request_id,
    const ObjectCreateRequest& request) {
    safe_line(request.name, "object name");
    safe_line(request.group_id, "group id");
    if (request.name.empty() ||
        request.name.size() > 256U) {
        throw std::invalid_argument(
            "Object name must contain 1..256 bytes");
    }

    std::string payload;
    payload.reserve(320U);
    payload += "name=" + request.name + "\n";
    append_transform(payload, request.transform);
    payload +=
        "physical=" +
        std::string(request.physical ? "true" : "false") +
        "\ngroup_id=" + request.group_id +
        "\ngroup_permissions=" +
        std::to_string(request.group_permissions) +
        "\neveryone_permissions=" +
        std::to_string(request.everyone_permissions) +
        "\n";

    return {
        .type = MessageType::entity_create,
        .request_id = request_id,
        .payload = payload_from_string(payload),
    };
}

Frame make_object_update(
    std::uint32_t request_id,
    std::uint64_t entity_id,
    const SceneTransform& transform) {
    require_id(entity_id, "entity id");
    std::string payload =
        "id=" + std::to_string(entity_id) + "\n";
    append_transform(payload, transform);
    return {
        .type = MessageType::entity_update,
        .request_id = request_id,
        .payload = payload_from_string(payload),
    };
}

Frame make_object_delete(
    std::uint32_t request_id,
    std::uint64_t entity_id) {
    require_id(entity_id, "entity id");
    return {
        .type = MessageType::entity_delete,
        .request_id = request_id,
        .payload = payload_from_string(
            "id=" + std::to_string(entity_id) + "\n"),
    };
}

Frame make_object_permissions(
    std::uint32_t request_id,
    const ObjectPermissionsRequest& request) {
    require_id(request.entity_id, "entity id");
    safe_line(request.group_id, "group id");
    const auto payload =
        "id=" + std::to_string(request.entity_id) +
        "\ngroup_id=" + request.group_id +
        "\ngroup_permissions=" +
        std::to_string(request.group_permissions) +
        "\neveryone_permissions=" +
        std::to_string(request.everyone_permissions) +
        "\n";
    return {
        .type = MessageType::entity_permissions,
        .request_id = request_id,
        .payload = payload_from_string(payload),
    };
}

Frame make_object_link(
    std::uint32_t request_id,
    std::uint64_t root_id,
    std::uint64_t child_id,
    bool unlink) {
    require_id(root_id, "root id");
    require_id(child_id, "child id");
    return {
        .type = MessageType::entity_link,
        .request_id = request_id,
        .payload = payload_from_string(
            "action=" +
            std::string(unlink ? "unlink" : "link") +
            "\nroot_id=" + std::to_string(root_id) +
            "\nchild_id=" + std::to_string(child_id) +
            "\n"),
    };
}

Frame make_object_text(
    std::uint32_t request_id,
    std::uint64_t entity_id,
    std::string_view text) {
    require_id(entity_id, "entity id");
    safe_line(text, "floating text");
    if (text.size() > 512U) {
        throw std::invalid_argument(
            "Floating text exceeds 512 bytes");
    }
    return {
        .type = MessageType::entity_text,
        .request_id = request_id,
        .payload = payload_from_string(
            "id=" + std::to_string(entity_id) +
            "\ntext=" + std::string(text) + "\n"),
    };
}

Frame make_object_motion(
    std::uint32_t request_id,
    const ObjectMotionRequest& request) {
    require_id(request.entity_id, "entity id");
    const auto payload =
        "id=" + std::to_string(request.entity_id) +
        "\nvx=" + number(request.vx, "vx") +
        "\nvy=" + number(request.vy, "vy") +
        "\nvz=" + number(request.vz, "vz") +
        "\navx=" + number(request.avx, "avx") +
        "\navy=" + number(request.avy, "avy") +
        "\navz=" + number(request.avz, "avz") +
        "\n";
    return {
        .type = MessageType::entity_motion,
        .request_id = request_id,
        .payload = payload_from_string(payload),
    };
}

Frame make_object_physics(
    std::uint32_t request_id,
    std::uint64_t entity_id,
    std::string_view action,
    std::string_view additional_fields) {
    require_id(entity_id, "entity id");
    safe_line(action, "physics action");
    if (action.empty()) {
        throw std::invalid_argument(
            "Physics action must not be empty");
    }
    if (!additional_fields.empty() &&
        additional_fields.back() != '\n') {
        throw std::invalid_argument(
            "Physics additional fields must end in newline");
    }

    return {
        .type = MessageType::entity_physics,
        .request_id = request_id,
        .payload = payload_from_string(
            "id=" + std::to_string(entity_id) +
            "\naction=" + std::string(action) +
            "\n" + std::string(additional_fields)),
    };
}

Frame make_object_interact(
    std::uint32_t request_id,
    std::uint64_t entity_id,
    std::string_view phase) {
    require_id(entity_id, "entity id");
    if (phase != "start" &&
        phase != "touch" &&
        phase != "end") {
        throw std::invalid_argument(
            "Interaction phase must be start, touch or end");
    }
    return {
        .type = MessageType::entity_interact,
        .request_id = request_id,
        .payload = payload_from_string(
            "id=" + std::to_string(entity_id) +
            "\nphase=" + std::string(phase) + "\n"),
    };
}

Frame make_parcel_info_request(
    std::uint32_t request_id,
    std::optional<double> x,
    std::optional<double> y) {
    if (x.has_value() != y.has_value()) {
        throw std::invalid_argument(
            "Parcel point query requires both x and y");
    }

    std::string payload;
    if (x.has_value()) {
        payload =
            "x=" + number(*x, "parcel x") +
            "\ny=" + number(*y, "parcel y") +
            "\n";
    }

    return {
        .type = MessageType::parcel_info_request,
        .request_id = request_id,
        .payload = payload_from_string(payload),
    };
}

Frame make_terrain_set_request(
    std::uint32_t request_id,
    std::size_t grid_x,
    std::size_t grid_y,
    double height) {
    if (grid_x >
            static_cast<std::size_t>(
                std::numeric_limits<std::uint64_t>::max()) ||
        grid_y >
            static_cast<std::size_t>(
                std::numeric_limits<std::uint64_t>::max())) {
        throw std::invalid_argument(
            "Terrain grid coordinate is out of range");
    }

    return {
        .type = MessageType::terrain_set_request,
        .request_id = request_id,
        .payload = payload_from_string(
            "x=" + std::to_string(grid_x) +
            "\ny=" + std::to_string(grid_y) +
            "\nheight=" + number(height, "terrain height") +
            "\n"),
    };
}

SceneCommandAck parse_scene_command_ack(
    const Frame& frame,
    MessageType expected_type,
    std::uint32_t expected_request_id) {
    if (frame.type == MessageType::error) {
        const auto error = parse_scene_error(frame);
        throw std::runtime_error(
            "Scene command failed: " + error.reason);
    }
    if (frame.type != expected_type ||
        frame.request_id != expected_request_id) {
        throw std::runtime_error(
            "Scene command response mismatch");
    }

    const auto values =
        parse_key_value_payload(
            payload_as_string(frame));

    SceneCommandAck result;
    if (const auto it = values.find("status");
        it != values.end()) {
        result.status = it->second;
    }
    if (const auto it = values.find("id");
        it != values.end() && !it->second.empty()) {
        result.entity_id =
            parse_unsigned<std::uint64_t>(
                it->second,
                "Scene entity id");
    }
    if (const auto it = values.find("sequence");
        it != values.end() && !it->second.empty()) {
        result.sequence =
            parse_unsigned<std::uint64_t>(
                it->second,
                "Scene sequence");
    }
    if (const auto it = values.find("revision");
        it != values.end() && !it->second.empty()) {
        result.revision =
            parse_unsigned<std::uint64_t>(
                it->second,
                "terrain revision");
    }
    return result;
}

ParcelInfoResult parse_parcel_info(
    const Frame& frame,
    std::uint32_t expected_request_id) {
    if (frame.type == MessageType::error) {
        const auto error = parse_scene_error(frame);
        throw std::runtime_error(
            "Parcel query failed: " + error.reason);
    }
    if (frame.type != MessageType::parcel_info ||
        frame.request_id != expected_request_id) {
        throw std::runtime_error(
            "Parcel response mismatch");
    }

    ParcelInfoResult result;
    std::size_t declared_count = 0U;
    const auto payload = payload_as_string(frame);
    std::size_t start = 0U;

    while (start < payload.size()) {
        const auto end = payload.find('\n', start);
        auto line = std::string_view(payload).substr(
            start,
            end == std::string::npos
                ? std::string_view::npos
                : end - start);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1U);
        }

        if (line.starts_with("mode=")) {
            result.mode = std::string(line.substr(5U));
        } else if (line.starts_with("count=")) {
            declared_count =
                parse_unsigned<std::size_t>(
                    line.substr(6U),
                    "parcel count");
        } else if (line.starts_with("parcel=")) {
            const auto fields =
                split(line.substr(7U), '|');
            if (fields.size() != 12U) {
                throw std::runtime_error(
                    "Parcel response field count mismatch");
            }

            ParcelRecord parcel;
            parcel.id = std::string(fields[0]);
            parcel.name = std::string(fields[1]);
            parcel.owner_user_id =
                std::string(fields[2]);
            parcel.group_id =
                std::string(fields[3]);
            parcel.x1 =
                parse_unsigned<std::uint16_t>(
                    fields[4], "parcel x1");
            parcel.y1 =
                parse_unsigned<std::uint16_t>(
                    fields[5], "parcel y1");
            parcel.x2 =
                parse_unsigned<std::uint16_t>(
                    fields[6], "parcel x2");
            parcel.y2 =
                parse_unsigned<std::uint16_t>(
                    fields[7], "parcel y2");
            parcel.public_entry =
                parse_bool01(
                    fields[8],
                    "parcel public_entry");
            parcel.public_build =
                parse_bool01(
                    fields[9],
                    "parcel public_build");
            parcel.group_build =
                parse_bool01(
                    fields[10],
                    "parcel group_build");
            parcel.group_terraform =
                parse_bool01(
                    fields[11],
                    "parcel group_terraform");
            result.parcels.push_back(
                std::move(parcel));
        }

        if (end == std::string::npos) {
            break;
        }
        start = end + 1U;
    }

    if (result.mode != "point" &&
        result.mode != "region") {
        throw std::runtime_error(
            "Parcel response mode is invalid");
    }
    if (declared_count != result.parcels.size()) {
        throw std::runtime_error(
            "Parcel response count mismatch");
    }
    return result;
}

} // namespace ogl::viewer::scene
