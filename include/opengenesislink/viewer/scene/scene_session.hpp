#pragma once

#include "opengenesislink/viewer/scene/avatar_protocol.hpp"
#include "opengenesislink/viewer/scene/session_protocol.hpp"
#include "opengenesislink/viewer/scene/terrain_protocol.hpp"
#include "opengenesislink/viewer/scene/world_actions.hpp"
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
        std::string_view scene_ticket,
        std::uint64_t initial_sync_since = 0U);

    [[nodiscard]] Frame request_sync(
        std::uint64_t since,
        std::uint32_t max_events = 256U);

    [[nodiscard]] TerrainSample request_terrain_sample(
        double x,
        double y);

    [[nodiscard]] AvatarReconcileAck reconcile_avatar(
        const AvatarReconcileRequest& request);

    [[nodiscard]] SceneCommandAck send_chat(
        std::string_view text);

    [[nodiscard]] SceneCommandAck create_object(
        const ObjectCreateRequest& request);

    [[nodiscard]] SceneCommandAck update_object(
        std::uint64_t entity_id,
        const SceneTransform& transform);

    [[nodiscard]] SceneCommandAck delete_object(
        std::uint64_t entity_id);

    [[nodiscard]] SceneCommandAck update_object_permissions(
        const ObjectPermissionsRequest& request);

    [[nodiscard]] SceneCommandAck link_object(
        std::uint64_t root_id,
        std::uint64_t child_id,
        bool unlink = false);

    [[nodiscard]] SceneCommandAck set_object_text(
        std::uint64_t entity_id,
        std::string_view text);

    [[nodiscard]] SceneCommandAck set_object_motion(
        const ObjectMotionRequest& request);

    [[nodiscard]] SceneCommandAck object_physics(
        std::uint64_t entity_id,
        std::string_view action,
        std::string_view additional_fields = {});

    [[nodiscard]] SceneCommandAck interact_object(
        std::uint64_t entity_id,
        std::string_view phase);

    [[nodiscard]] ParcelInfoResult request_parcel_info(
        std::optional<double> x = std::nullopt,
        std::optional<double> y = std::nullopt);

    [[nodiscard]] SceneCommandAck set_terrain_height(
        std::size_t grid_x,
        std::size_t grid_y,
        double height);

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
        std::string_view scene_ticket,
        std::uint64_t initial_sync_since = 0U);

    void disconnect() noexcept;
    [[nodiscard]] bool is_connected() const noexcept;
    [[nodiscard]] SceneSession& session() noexcept;

private:
    TcpByteStream stream_;
    FrameChannel channel_;
    SceneSession session_;
};

} // namespace ogl::viewer::scene
