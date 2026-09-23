#pragma once

#include "opengenesislink/viewer/app/login_flow.hpp"
#include "opengenesislink/viewer/core/curl_http_transport.hpp"
#include "opengenesislink/viewer/scene/scene_session.hpp"
#include "opengenesislink/viewer/world/render_world.hpp"
#include "opengenesislink/viewer/world/scene_synchronizer.hpp"
#include "opengenesislink/viewer/world/world_model.hpp"

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

    void disconnect() noexcept;

    [[nodiscard]] bool connected() const noexcept;
    [[nodiscard]] const ConnectionInfo& connection_info() const;
    [[nodiscard]] const world::RenderRegion& render_region() const;
    [[nodiscard]] const world::WorldModel& world_model() const noexcept;

private:
    core::CurlHttpTransport http_;
    CoreEntryCoordinator core_entry_;
    scene::SceneConnection scene_;
    world::WorldModel world_;
    world::SceneSynchronizer synchronizer_;
    world::RenderWorldBuilder render_builder_;
    std::optional<world::RenderRegion> render_region_;
    std::optional<ConnectionInfo> info_;
    std::string bearer_token_;
    std::string core_base_url_;
    std::string region_id_;
    double spawn_x_ = 128.0;
    double spawn_y_ = 128.0;
    double spawn_z_ = 25.0;
};

} // namespace ogl::viewer::app
