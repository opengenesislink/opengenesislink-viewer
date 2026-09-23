#pragma once

#include "opengenesislink/viewer/scene/session_protocol.hpp"
#include "opengenesislink/viewer/scene/terrain_protocol.hpp"
#include "opengenesislink/viewer/scene/transport.hpp"

#include <cstdint>
#include <deque>
#include <string_view>

namespace ogl::viewer::scene {

struct SceneStartupResult {
    HelloAck hello;
    SceneJoinAck join;
    Frame initial_sync;
};

class SceneSession {
public:
    explicit SceneSession(FrameChannel& channel);

    [[nodiscard]] SceneStartupResult start(
        std::string_view region_id,
        std::string_view scene_ticket);

    [[nodiscard]] Frame request_sync(
        std::uint64_t since,
        std::uint32_t max_events = 256U);

    [[nodiscard]] TerrainSample request_terrain_sample(
        double x,
        double y);

    [[nodiscard]] Frame receive_next();
    void disconnect() noexcept;
    [[nodiscard]] bool is_connected() const noexcept;

private:
    [[nodiscard]] std::uint32_t allocate_request_id();
    [[nodiscard]] Frame receive_correlated(
        MessageType expected_type,
        std::uint32_t request_id);

    FrameChannel& channel_;
    std::uint32_t next_request_id_ = 1U;
    std::deque<Frame> deferred_;
    bool joined_ = false;
};

class SceneConnection {
public:
    explicit SceneConnection(
        std::chrono::milliseconds io_timeout = std::chrono::seconds(15));
    ~SceneConnection();

    SceneConnection(const SceneConnection&) = delete;
    SceneConnection& operator=(const SceneConnection&) = delete;

    [[nodiscard]] SceneStartupResult connect_and_enter(
        std::string_view scene_endpoint,
        std::string_view region_id,
        std::string_view scene_ticket);

    void disconnect() noexcept;
    [[nodiscard]] bool is_connected() const noexcept;
    [[nodiscard]] SceneSession& session() noexcept;

private:
    TcpByteStream stream_;
    FrameChannel channel_;
    SceneSession session_;
};

} // namespace ogl::viewer::scene
