#include "opengenesislink/viewer/world/world_model.hpp"

#include "opengenesislink/viewer/scene/frame.hpp"

#include <charconv>
#include <limits>
#include <map>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace ogl::viewer::world {
namespace {

struct ParsedLines {
    std::map<std::string, std::string> scalar;
    std::vector<std::string> entities;
    std::vector<std::string> events;
};

ParsedLines parse_lines(std::string_view payload) {
    ParsedLines result;
    std::size_t start = 0;

    while (start < payload.size()) {
        const auto end = payload.find('\n', start);
        auto line = payload.substr(
            start,
            end == std::string_view::npos
                ? payload.size() - start
                : end - start);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1U);
        }

        if (!line.empty()) {
            const auto separator = line.find('=');
            if (separator == std::string_view::npos || separator == 0U) {
                throw std::runtime_error("Invalid SCENE_SYNC line");
            }

            const auto key = line.substr(0U, separator);
            const auto value = line.substr(separator + 1U);
            if (key == "entity") {
                result.entities.emplace_back(value);
            } else if (key == "event") {
                result.events.emplace_back(value);
            } else {
                result.scalar[std::string(key)] = std::string(value);
            }
        }

        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1U;
    }

    return result;
}

std::vector<std::string_view> split_fields(std::string_view value) {
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    while (true) {
        const auto separator = value.find('|', start);
        fields.push_back(value.substr(
            start,
            separator == std::string_view::npos
                ? std::string_view::npos
                : separator - start));
        if (separator == std::string_view::npos) {
            break;
        }
        start = separator + 1U;
    }
    return fields;
}

const std::string& required(
    const std::map<std::string, std::string>& values,
    std::string_view key) {
    const auto it = values.find(std::string(key));
    if (it == values.end() || it->second.empty()) {
        throw std::runtime_error(
            "SCENE_SYNC is missing field: " + std::string(key));
    }
    return it->second;
}

template <typename T>
T parse_unsigned(std::string_view value, std::string_view label) {
    T result{};
    const auto [ptr, error] =
        std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || ptr != value.data() + value.size()) {
        throw std::runtime_error(
            "Invalid unsigned SCENE_SYNC field: " + std::string(label));
    }
    return result;
}

template <typename T>
T parse_required_unsigned(
    const std::map<std::string, std::string>& values,
    std::string_view key) {
    return parse_unsigned<T>(required(values, key), key);
}

double parse_double(std::string_view value, std::string_view label) {
    double result = 0.0;
    const auto [ptr, error] =
        std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || ptr != value.data() + value.size()) {
        throw std::runtime_error(
            "Invalid floating-point SCENE_SYNC field: " + std::string(label));
    }
    return result;
}

double parse_required_double(
    const std::map<std::string, std::string>& values,
    std::string_view key) {
    return parse_double(required(values, key), key);
}

Entity parse_entity(std::string_view value) {
    const auto fields = split_fields(value);
    if (fields.size() != 31U) {
        throw std::runtime_error("SCENE_SYNC entity field count mismatch");
    }

    Entity entity;
    entity.id = parse_unsigned<std::uint64_t>(fields[0], "entity.id");
    if (entity.id == 0U) {
        throw std::runtime_error("SCENE_SYNC entity id must not be zero");
    }

    if (fields[1] == "object") {
        entity.kind = EntityKind::object;
    } else if (fields[1] == "avatar") {
        entity.kind = EntityKind::avatar;
    } else {
        throw std::runtime_error("Unknown SCENE_SYNC entity kind");
    }

    entity.name = std::string(fields[2]);
    entity.transform.position = {
        parse_double(fields[3], "entity.position.x"),
        parse_double(fields[4], "entity.position.y"),
        parse_double(fields[5], "entity.position.z")};
    entity.transform.rotation = {
        parse_double(fields[6], "entity.rotation.x"),
        parse_double(fields[7], "entity.rotation.y"),
        parse_double(fields[8], "entity.rotation.z")};
    entity.transform.scale = {
        parse_double(fields[9], "entity.scale.x"),
        parse_double(fields[10], "entity.scale.y"),
        parse_double(fields[11], "entity.scale.z")};
    entity.owner_user_id = std::string(fields[12]);
    entity.group_id = std::string(fields[13]);
    entity.owner_permissions =
        parse_unsigned<std::uint32_t>(fields[14], "entity.owner_permissions");
    entity.group_permissions =
        parse_unsigned<std::uint32_t>(fields[15], "entity.group_permissions");
    entity.everyone_permissions =
        parse_unsigned<std::uint32_t>(fields[16], "entity.everyone_permissions");
    entity.parent_entity_id =
        parse_unsigned<std::uint64_t>(fields[17], "entity.parent_entity_id");
    entity.link_number =
        parse_unsigned<std::uint32_t>(fields[18], "entity.link_number");
    entity.physics.physical =
        parse_unsigned<std::uint32_t>(fields[19], "entity.physical") != 0U;
    entity.floating_text = std::string(fields[20]);
    entity.physics.velocity = {
        parse_double(fields[21], "entity.velocity.x"),
        parse_double(fields[22], "entity.velocity.y"),
        parse_double(fields[23], "entity.velocity.z")};
    entity.physics.angular_velocity = {
        parse_double(fields[24], "entity.angular_velocity.x"),
        parse_double(fields[25], "entity.angular_velocity.y"),
        parse_double(fields[26], "entity.angular_velocity.z")};
    entity.physics.mass = parse_double(fields[27], "entity.mass");
    entity.physics.restitution = parse_double(fields[28], "entity.restitution");
    entity.physics.friction = parse_double(fields[29], "entity.friction");
    entity.physics.buoyancy = parse_double(fields[30], "entity.buoyancy");
    return entity;
}

SceneEvent parse_event(std::string_view value) {
    const auto fields = split_fields(value);
    if (fields.size() != 13U) {
        throw std::runtime_error("SCENE_SYNC event field count mismatch");
    }

    SceneEvent event;
    event.sequence = parse_unsigned<std::uint64_t>(fields[0], "event.sequence");
    event.type = std::string(fields[1]);
    event.entity_id = parse_unsigned<std::uint64_t>(fields[2], "event.entity_id");
    event.text = std::string(fields[3]);
    event.transform.position = {
        parse_double(fields[4], "event.position.x"),
        parse_double(fields[5], "event.position.y"),
        parse_double(fields[6], "event.position.z")};
    event.transform.rotation = {
        parse_double(fields[7], "event.rotation.x"),
        parse_double(fields[8], "event.rotation.y"),
        parse_double(fields[9], "event.rotation.z")};
    event.transform.scale = {
        parse_double(fields[10], "event.scale.x"),
        parse_double(fields[11], "event.scale.y"),
        parse_double(fields[12], "event.scale.z")};
    return event;
}

RegionState parse_snapshot(const ParsedLines& lines) {
    RegionState state;
    state.region_id = required(lines.scalar, "region");
    state.sequence =
        parse_required_unsigned<std::uint64_t>(lines.scalar, "sequence");
    state.terrain_width =
        parse_required_unsigned<std::size_t>(lines.scalar, "terrain_width");
    state.terrain_height =
        parse_required_unsigned<std::size_t>(lines.scalar, "terrain_height");
    state.terrain_cell_size =
        parse_required_double(lines.scalar, "terrain_cell_size");
    state.terrain_revision =
        parse_required_unsigned<std::uint64_t>(lines.scalar, "terrain_revision");
    state.water_height =
        parse_required_double(lines.scalar, "water_height");

    const auto entity_count =
        parse_required_unsigned<std::size_t>(lines.scalar, "entity_count");
    if (entity_count != lines.entities.size()) {
        throw std::runtime_error("SCENE_SYNC snapshot entity_count mismatch");
    }

    for (const auto& encoded : lines.entities) {
        auto entity = parse_entity(encoded);
        const auto id = entity.id;
        if (!state.entities.emplace(id, std::move(entity)).second) {
            throw std::runtime_error("SCENE_SYNC snapshot contains duplicate entity id");
        }
    }
    return state;
}

SceneDelta parse_delta(const ParsedLines& lines) {
    SceneDelta delta;
    delta.region_id = required(lines.scalar, "region");
    delta.from = parse_required_unsigned<std::uint64_t>(lines.scalar, "from");
    delta.latest = parse_required_unsigned<std::uint64_t>(lines.scalar, "latest");

    const auto count =
        parse_required_unsigned<std::size_t>(lines.scalar, "count");
    if (count != lines.events.size()) {
        throw std::runtime_error("SCENE_SYNC delta count mismatch");
    }

    delta.events.reserve(lines.events.size());
    for (const auto& encoded : lines.events) {
        delta.events.push_back(parse_event(encoded));
    }
    return delta;
}

bool requires_full_entity_refresh(std::string_view type) {
    return
        type == "entity_created" ||
        type == "entity_restored" ||
        type == "entity_linked" ||
        type == "entity_unlinked" ||
        type == "permissions_updated" ||
        type == "physics_updated" ||
        type == "physics_shape" ||
        type == "physics_material" ||
        type == "physics_force" ||
        type == "physics_impulse" ||
        type == "physics_torque" ||
        type == "physics_angular_impulse" ||
        type == "physics_buoyancy" ||
        type == "physics_constraint" ||
        type == "physics_spring" ||
        type == "character_config" ||
        type == "character_jump";
}

} // namespace

SceneSyncPayload parse_scene_sync(const scene::Frame& frame) {
    if (frame.type != scene::MessageType::scene_sync) {
        throw std::runtime_error("Frame is not SCENE_SYNC");
    }

    const auto lines = parse_lines(scene::payload_as_string(frame));
    const auto mode = required(lines.scalar, "mode");

    SceneSyncPayload result;
    if (mode == "snapshot") {
        result.mode = SceneSyncMode::snapshot;
        result.snapshot = parse_snapshot(lines);
    } else if (mode == "delta") {
        result.mode = SceneSyncMode::delta;
        result.delta = parse_delta(lines);
    } else {
        throw std::runtime_error("Unknown SCENE_SYNC mode");
    }
    return result;
}

SyncApplyResult WorldModel::apply(const SceneSyncPayload& sync) {
    if (sync.mode == SceneSyncMode::snapshot) {
        if (!sync.snapshot.has_value()) {
            throw std::runtime_error("Snapshot sync has no snapshot payload");
        }
        region_ = *sync.snapshot;
        initialized_ = true;
        return {
            .applied = true,
            .requires_snapshot = false,
            .events_applied = 0U,
        };
    }

    if (!sync.delta.has_value()) {
        throw std::runtime_error("Delta sync has no delta payload");
    }
    if (!initialized_) {
        return {
            .applied = false,
            .requires_snapshot = true,
            .events_applied = 0U,
        };
    }

    const auto& delta = *sync.delta;
    if (delta.region_id != region_.region_id ||
        delta.from != region_.sequence ||
        delta.latest < delta.from) {
        return {
            .applied = false,
            .requires_snapshot = true,
            .events_applied = 0U,
        };
    }

    auto expected_sequence = region_.sequence + 1U;
    for (const auto& event : delta.events) {
        if (event.sequence != expected_sequence) {
            return {
                .applied = false,
                .requires_snapshot = true,
                .events_applied = 0U,
            };
        }
        expected_sequence = event.sequence + 1U;
    }

    if (delta.events.empty()) {
        if (delta.latest != delta.from) {
            return {
                .applied = false,
                .requires_snapshot = true,
                .events_applied = 0U,
            };
        }
    } else if (delta.events.back().sequence > delta.latest) {
        return {
            .applied = false,
            .requires_snapshot = true,
            .events_applied = 0U,
        };
    }

    SyncApplyResult result{
        .applied = true,
        .requires_snapshot = false,
        .events_applied = 0U,
    };

    for (const auto& event : delta.events) {
        if (!apply_event(event)) {
            result.requires_snapshot = true;
        }
        region_.sequence = event.sequence;
        ++result.events_applied;
    }

    return result;
}

bool WorldModel::apply_event(const SceneEvent& event) {
    if (event.type == "chat" ||
        event.type == "chat_whisper" ||
        event.type == "chat_shout") {
        std::string sender_name;
        if (const auto sender =
                region_.entities.find(event.entity_id);
            sender != region_.entities.end()) {
            sender_name = sender->second.name;
        }
        if (sender_name.empty()) {
            sender_name = event.entity_id == 0U
                ? std::string{"System"}
                : std::string{"Avatar #"} +
                      std::to_string(event.entity_id);
        }

        chat_history_.push_back({
            .sequence = event.sequence,
            .sender_entity_id = event.entity_id,
            .sender_name = std::move(sender_name),
            .kind = event.type,
            .text = event.text,
        });
        constexpr std::size_t max_chat_messages = 100U;
        if (chat_history_.size() > max_chat_messages) {
            chat_history_.erase(
                chat_history_.begin(),
                chat_history_.begin() +
                    static_cast<std::ptrdiff_t>(
                        chat_history_.size() -
                        max_chat_messages));
        }
        return true;
    }

    if (event.type == "entity_deleted") {
        region_.entities.erase(event.entity_id);
        return true;
    }

    if (event.type == "terrain_updated") {
        if (!event.text.empty()) {
            try {
                region_.terrain_revision =
                    parse_unsigned<std::uint64_t>(event.text, "terrain_revision");
            } catch (const std::exception&) {
                return false;
            }
        }
        return true;
    }

    const auto entity = region_.entities.find(event.entity_id);

    if (event.type == "entity_updated" || event.type == "avatar_move") {
        if (entity == region_.entities.end()) {
            return false;
        }
        entity->second.transform = event.transform;
        return true;
    }

    if (event.type == "object_text_updated") {
        if (entity == region_.entities.end()) {
            return false;
        }
        entity->second.transform = event.transform;
        entity->second.floating_text = event.text;
        return true;
    }

    if (requires_full_entity_refresh(event.type)) {
        return false;
    }

    // Transient collision/touch/boundary/script events do not mutate the
    // persistent render model. They remain consumable by higher-level systems.
    return true;
}

bool WorldModel::initialized() const noexcept {
    return initialized_;
}

const RegionState& WorldModel::region() const {
    if (!initialized_) {
        throw std::runtime_error("WorldModel is not initialized");
    }
    return region_;
}

std::uint64_t WorldModel::sequence() const noexcept {
    return initialized_ ? region_.sequence : 0U;
}

const std::vector<ChatMessage>&
WorldModel::chat_history() const noexcept {
    return chat_history_;
}

bool WorldModel::apply_reconciled_avatar(
    std::uint64_t entity_id,
    const Transform& transform,
    const Vec3& velocity) {
    if (!initialized_) {
        return false;
    }

    const auto found = region_.entities.find(entity_id);
    if (found == region_.entities.end() ||
        found->second.kind != EntityKind::avatar) {
        return false;
    }

    found->second.transform = transform;
    found->second.physics.velocity = velocity;
    return true;
}

void WorldModel::clear() noexcept {
    region_ = {};
    chat_history_.clear();
    initialized_ = false;
}

} // namespace ogl::viewer::world
