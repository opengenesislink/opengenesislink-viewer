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
    std::string_view scene_ticket) {
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

    auto initial_sync = request_sync(0U, 256U);
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
    std::string_view scene_ticket) {
    disconnect();
    try {
        stream_.connect(parse_scene_endpoint(scene_endpoint));
        return session_.start(region_id, scene_ticket);
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
