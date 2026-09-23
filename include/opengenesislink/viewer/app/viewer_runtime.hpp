#pragma once

#include "opengenesislink/viewer/app/login_flow.hpp"
#include "opengenesislink/viewer/core/curl_http_transport.hpp"
#include "opengenesislink/viewer/input/avatar_controller.hpp"
#include "opengenesislink/viewer/scene/scene_session.hpp"
#include "opengenesislink/viewer/world/render_world.hpp"
#include "opengenesislink/viewer/world/scene_synchronizer.hpp"
#include "opengenesislink/viewer/world/terrain_refinement.hpp"
#include "opengenesislink/viewer/world/world_model.hpp"

#include <cstddef>
#include <cstdint>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace ogl::viewer::app {

struct ConnectionInfo {
    std::string server_version;
    std::string channel;
    std::string user_id;
    std::string username;
    std::string display_name;
    std::string region_id;
    std::string scene_endpoint;
    std::vector<std::string> scene_capabilities;
    std::optional<core::SpawnPoint> spawn;
    std::uint64_t avatar_id = 0;
    bool avatar_reconcile_supported = false;
};

struct RuntimeBackgroundState {
    std::size_t terrain_resolution = 0;
    bool terrain_refining = false;
    bool movement_pending = false;
    std::string boundary;
    std::string last_error;
};

class ViewerRuntime {
public:
    ViewerRuntime();
    ~ViewerRuntime();

    ViewerRuntime(const ViewerRuntime&) = delete;
    ViewerRuntime& operator=(const ViewerRuntime&) = delete;

    [[nodiscard]] ConnectionInfo connect(
        const LoginRequest& request);

    [[nodiscard]] world::SynchronizeResult poll(
        std::uint32_t max_events = 256U);

    [[nodiscard]] ConnectionInfo reconnect();

    void service_background();

    [[nodiscard]] bool queue_avatar_control(
        const input::AvatarControlInput& input);

    void disconnect() noexcept;

    [[nodiscard]] bool connected() const noexcept;
    [[nodiscard]] const ConnectionInfo& connection_info() const;
    [[nodiscard]] const world::RenderRegion& render_region() const;
    [[nodiscard]] const world::WorldModel& world_model() const noexcept;
    [[nodiscard]] std::uint64_t avatar_id() const noexcept;
    [[nodiscard]] RuntimeBackgroundState background_state() const;

private:
    struct TerrainTaskResult {
        world::TerrainGridPoint point;
        std::uint64_t expected_revision = 0;
        scene::TerrainSample sample;
    };

    void rebuild_render_region();
    void launch_terrain_sample();
    void launch_movement();
    void wait_for_background() noexcept;

    [[nodiscard]] bool has_scene_capability(
        const std::string& capability) const;

    core::CurlHttpTransport http_;
    CoreEntryCoordinator core_entry_;
    scene::SceneConnection scene_;
    world::WorldModel world_;
    world::SceneSynchronizer synchronizer_;
    world::RenderWorldBuilder render_builder_;
    world::TerrainRefinement terrain_refinement_;

    mutable std::mutex scene_io_mutex_;
    std::future<TerrainTaskResult> terrain_future_;
    bool terrain_pending_ = false;
    bool terrain_suspended_ = false;

    std::future<scene::AvatarReconcileAck> movement_future_;
    bool movement_pending_ = false;
    std::optional<scene::AvatarReconcileRequest> queued_movement_;

    std::optional<world::RenderRegion> render_region_;
    std::optional<ConnectionInfo> info_;

    std::string bearer_token_;
    std::string core_base_url_;
    std::string region_id_;
    double spawn_x_ = 128.0;
    double spawn_y_ = 128.0;
    double spawn_z_ = 25.0;

    std::uint64_t avatar_id_ = 0;
    std::uint64_t next_client_sequence_ = 1U;
    bool can_reconcile_avatar_ = false;
    std::string last_boundary_;
    std::string background_error_;
};

} // namespace ogl::viewer::app
