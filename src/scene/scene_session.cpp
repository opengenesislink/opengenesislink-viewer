#include "opengenesislink/viewer/scene/scene_session.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace ogl::viewer::scene {

SceneSession::SceneSession(FrameChannel& channel)
    : channel_(channel) {}

std::uint32_t SceneSession::allocate_request_id() {
    auto result = next_request_id_++;
    if (result == 0U) {
        result = next_request_id_++;
    }
    if (next_request_id_ == 0U) {
        next_request_id_ = 1U;
    }
    return result;
}

Frame SceneSession::receive_correlated(
    MessageType expected_type,
    std::uint32_t request_id) {
    while (true) {
        auto frame = channel_.receive();
        if (frame.request_id != request_id) {
            deferred_.push_back(std::move(frame));
            continue;
        }

        if (frame.type == MessageType::error) {
            const auto error = parse_scene_error(frame);
            auto message = "Scene request failed: " + error.reason;
            if (!error.capability.empty()) {
                message += " (" + error.capability + ")";
            }
            throw std::runtime_error(message);
        }

        if (frame.type != expected_type) {
            throw std::runtime_error("Scene response type does not match request");
        }
        return frame;
    }
}

SceneStartupResult SceneSession::start(
    std::string_view region_id,
    std::string_view scene_ticket,
    std::uint64_t initial_sync_since) {
    if (!channel_.is_open()) {
        throw std::runtime_error("Scene channel is not connected");
    }
    if (joined_) {
        throw std::runtime_error("Scene session is already joined");
    }

    deferred_.clear();

    const auto hello_request_id = allocate_request_id();
    channel_.send(make_hello(hello_request_id));
    const auto hello_frame =
        receive_correlated(MessageType::hello_ack, hello_request_id);
    auto hello = parse_hello_ack(hello_frame, hello_request_id);

    const auto join_request_id = allocate_request_id();
    channel_.send(make_scene_join(join_request_id, region_id, scene_ticket));
    const auto join_frame =
        receive_correlated(MessageType::scene_join_ack, join_request_id);
    auto join = parse_scene_join_ack(join_frame, join_request_id);
    joined_ = true;

    auto initial_sync = request_sync(initial_sync_since, 256U);
    return {
        .hello = std::move(hello),
        .join = std::move(join),
        .initial_sync = std::move(initial_sync),
    };
}

Frame SceneSession::request_sync(
    std::uint64_t since,
    std::uint32_t max_events) {
    if (!joined_) {
        throw std::runtime_error("Scene sync requires a joined session");
    }

    const auto request_id = allocate_request_id();
    channel_.send(make_scene_sync_request(request_id, since, max_events));
    return receive_correlated(MessageType::scene_sync, request_id);
}

TerrainSample SceneSession::request_terrain_sample(
    double x,
    double y) {
    if (!joined_) {
        throw std::runtime_error(
            "Terrain sampling requires a joined Scene session");
    }

    const auto request_id = allocate_request_id();
    channel_.send(make_terrain_sample_request(request_id, x, y));
    const auto response =
        receive_correlated(MessageType::terrain_sample, request_id);
    return parse_terrain_sample(response, request_id);
}

AvatarReconcileAck SceneSession::reconcile_avatar(
    const AvatarReconcileRequest& request) {
    if (!joined_) {
        throw std::runtime_error(
            "Avatar reconciliation requires a joined Scene session");
    }

    const auto request_id = allocate_request_id();
    channel_.send(
        make_avatar_reconcile(request_id, request));
    const auto response =
        receive_correlated(
            MessageType::avatar_reconcile_ack,
            request_id);
    return parse_avatar_reconcile_ack(
        response,
        request_id);
}

SceneCommandAck SceneSession::send_chat(
    std::string_view text) {
    if (!joined_) {
        throw std::runtime_error(
            "Local chat requires a joined Scene session");
    }
    const auto request_id = allocate_request_id();
    channel_.send(make_chat_send(request_id, text));
    return parse_scene_command_ack(
        receive_correlated(
            MessageType::chat_event,
            request_id),
        MessageType::chat_event,
        request_id);
}

SceneCommandAck SceneSession::create_object(
    const ObjectCreateRequest& request) {
    if (!joined_) {
        throw std::runtime_error(
            "Object creation requires a joined Scene session");
    }
    const auto request_id = allocate_request_id();
    channel_.send(make_object_create(request_id, request));
    return parse_scene_command_ack(
        receive_correlated(
            MessageType::entity_create_ack,
            request_id),
        MessageType::entity_create_ack,
        request_id);
}

SceneCommandAck SceneSession::update_object(
    std::uint64_t entity_id,
    const SceneTransform& transform) {
    if (!joined_) {
        throw std::runtime_error(
            "Object update requires a joined Scene session");
    }
    const auto request_id = allocate_request_id();
    channel_.send(
        make_object_update(
            request_id,
            entity_id,
            transform));
    return parse_scene_command_ack(
        receive_correlated(
            MessageType::entity_update_ack,
            request_id),
        MessageType::entity_update_ack,
        request_id);
}

SceneCommandAck SceneSession::delete_object(
    std::uint64_t entity_id) {
    if (!joined_) {
        throw std::runtime_error(
            "Object deletion requires a joined Scene session");
    }
    const auto request_id = allocate_request_id();
    channel_.send(
        make_object_delete(
            request_id,
            entity_id));
    return parse_scene_command_ack(
        receive_correlated(
            MessageType::entity_delete_ack,
            request_id),
        MessageType::entity_delete_ack,
        request_id);
}

SceneCommandAck SceneSession::update_object_permissions(
    const ObjectPermissionsRequest& request) {
    if (!joined_) {
        throw std::runtime_error(
            "Permission update requires a joined Scene session");
    }
    const auto request_id = allocate_request_id();
    channel_.send(
        make_object_permissions(
            request_id,
            request));
    return parse_scene_command_ack(
        receive_correlated(
            MessageType::entity_permissions_ack,
            request_id),
        MessageType::entity_permissions_ack,
        request_id);
}

SceneCommandAck SceneSession::link_object(
    std::uint64_t root_id,
    std::uint64_t child_id,
    bool unlink) {
    if (!joined_) {
        throw std::runtime_error(
            "Linkset update requires a joined Scene session");
    }
    const auto request_id = allocate_request_id();
    channel_.send(
        make_object_link(
            request_id,
            root_id,
            child_id,
            unlink));
    return parse_scene_command_ack(
        receive_correlated(
            MessageType::entity_link_ack,
            request_id),
        MessageType::entity_link_ack,
        request_id);
}

SceneCommandAck SceneSession::set_object_text(
    std::uint64_t entity_id,
    std::string_view text) {
    if (!joined_) {
        throw std::runtime_error(
            "Floating text update requires a joined Scene session");
    }
    const auto request_id = allocate_request_id();
    channel_.send(
        make_object_text(
            request_id,
            entity_id,
            text));
    return parse_scene_command_ack(
        receive_correlated(
            MessageType::entity_text_ack,
            request_id),
        MessageType::entity_text_ack,
        request_id);
}

SceneCommandAck SceneSession::set_object_motion(
    const ObjectMotionRequest& request) {
    if (!joined_) {
        throw std::runtime_error(
            "Object motion update requires a joined Scene session");
    }
    const auto request_id = allocate_request_id();
    channel_.send(
        make_object_motion(
            request_id,
            request));
    return parse_scene_command_ack(
        receive_correlated(
            MessageType::entity_motion_ack,
            request_id),
        MessageType::entity_motion_ack,
        request_id);
}

SceneCommandAck SceneSession::object_physics(
    std::uint64_t entity_id,
    std::string_view action,
    std::string_view additional_fields) {
    if (!joined_) {
        throw std::runtime_error(
            "Object Physics command requires a joined Scene session");
    }
    const auto request_id = allocate_request_id();
    channel_.send(
        make_object_physics(
            request_id,
            entity_id,
            action,
            additional_fields));
    return parse_scene_command_ack(
        receive_correlated(
            MessageType::entity_physics_ack,
            request_id),
        MessageType::entity_physics_ack,
        request_id);
}

SceneCommandAck SceneSession::interact_object(
    std::uint64_t entity_id,
    std::string_view phase) {
    if (!joined_) {
        throw std::runtime_error(
            "Object interaction requires a joined Scene session");
    }
    const auto request_id = allocate_request_id();
    channel_.send(
        make_object_interact(
            request_id,
            entity_id,
            phase));
    return parse_scene_command_ack(
        receive_correlated(
            MessageType::entity_interact_ack,
            request_id),
        MessageType::entity_interact_ack,
        request_id);
}

ParcelInfoResult SceneSession::request_parcel_info(
    std::optional<double> x,
    std::optional<double> y) {
    if (!joined_) {
        throw std::runtime_error(
            "Parcel query requires a joined Scene session");
    }
    const auto request_id = allocate_request_id();
    channel_.send(
        make_parcel_info_request(
            request_id,
            x,
            y));
    return parse_parcel_info(
        receive_correlated(
            MessageType::parcel_info,
            request_id),
        request_id);
}

SceneCommandAck SceneSession::set_terrain_height(
    std::size_t grid_x,
    std::size_t grid_y,
    double height) {
    if (!joined_) {
        throw std::runtime_error(
            "Terrain modification requires a joined Scene session");
    }
    const auto request_id = allocate_request_id();
    channel_.send(
        make_terrain_set_request(
            request_id,
            grid_x,
            grid_y,
            height));
    return parse_scene_command_ack(
        receive_correlated(
            MessageType::terrain_set_ack,
            request_id),
        MessageType::terrain_set_ack,
        request_id);
}

Frame SceneSession::receive_next() {
    if (!deferred_.empty()) {
        auto frame = std::move(deferred_.front());
        deferred_.pop_front();
        return frame;
    }
    return channel_.receive();
}

void SceneSession::disconnect() noexcept {
    if (channel_.is_open()) {
        try {
            channel_.send({
                .type = MessageType::goodbye,
                .request_id = allocate_request_id(),
                .payload = {},
            });
        } catch (...) {
        }
    }
    joined_ = false;
    deferred_.clear();
    channel_.close();
}

bool SceneSession::is_connected() const noexcept {
    return joined_ && channel_.is_open();
}

SceneConnection::SceneConnection(std::chrono::milliseconds io_timeout)
    : stream_(io_timeout),
      channel_(stream_),
      session_(channel_) {}

SceneConnection::~SceneConnection() {
    disconnect();
}

SceneStartupResult SceneConnection::connect_and_enter(
    std::string_view scene_endpoint,
    std::string_view region_id,
    std::string_view scene_ticket,
    std::uint64_t initial_sync_since) {
    disconnect();
    try {
        stream_.connect(parse_scene_endpoint(scene_endpoint));
        return session_.start(
            region_id,
            scene_ticket,
            initial_sync_since);
    } catch (...) {
        channel_.close();
        throw;
    }
}

void SceneConnection::disconnect() noexcept {
    session_.disconnect();
}

bool SceneConnection::is_connected() const noexcept {
    return session_.is_connected();
}

SceneSession& SceneConnection::session() noexcept {
    return session_;
}

} // namespace ogl::viewer::scene
