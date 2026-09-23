#include "opengenesislink/viewer/app/viewer_runtime.hpp"

#include <stdexcept>
#include <utility>

namespace ogl::viewer::app {

ViewerRuntime::ViewerRuntime()
    : http_{},
      core_entry_{http_},
      scene_{},
      world_{},
      synchronizer_{scene_.session(), world_},
      render_builder_{} {}

ViewerRuntime::~ViewerRuntime() {
    disconnect();
}

ConnectionInfo ViewerRuntime::connect(
    const LoginRequest& request) {
    disconnect();

    auto entry = core_entry_.enter(request);

    try {
        const auto startup = scene_.connect_and_enter(
            entry.bootstrap.scene_endpoint,
            entry.bootstrap.region_id,
            entry.bootstrap.scene_ticket);

        const auto synchronized =
            synchronizer_.apply(startup.initial_sync);
        if (!synchronized.applied || !world_.initialized()) {
            throw std::runtime_error(
                "Initial authoritative Scene state was not applied");
        }

        render_region_ = render_builder_.build(world_);
        bearer_token_ = entry.login.token;

        ConnectionInfo info;
        info.server_version = entry.discovery.release.server_version;
        info.channel = entry.discovery.release.channel;
        info.user_id = entry.login.user.id;
        info.username = entry.login.user.username;
        info.display_name = entry.login.user.display_name;
        info.region_id = entry.bootstrap.region_id;
        info.scene_endpoint = entry.bootstrap.scene_endpoint;
        info.scene_capabilities = entry.bootstrap.capabilities;
        info.spawn = entry.bootstrap.spawn;

        info_ = info;
        return info;
    } catch (...) {
        disconnect();
        throw;
    }
}

world::SynchronizeResult ViewerRuntime::poll(
    std::uint32_t max_events) {
    if (!connected()) {
        throw std::runtime_error(
            "Viewer runtime is not connected");
    }

    const auto result = synchronizer_.poll(max_events);
    if (result.applied) {
        render_region_ = render_builder_.build(world_);
    }
    return result;
}

void ViewerRuntime::disconnect() noexcept {
    scene_.disconnect();
    world_.clear();
    render_region_.reset();
    info_.reset();

    if (!bearer_token_.empty()) {
        std::fill(
            bearer_token_.begin(),
            bearer_token_.end(),
            '\0');
        bearer_token_.clear();
    }
}

bool ViewerRuntime::connected() const noexcept {
    return scene_.is_connected() &&
           world_.initialized() &&
           render_region_.has_value() &&
           info_.has_value();
}

const ConnectionInfo& ViewerRuntime::connection_info() const {
    if (!info_.has_value()) {
        throw std::runtime_error(
            "Viewer runtime has no active connection");
    }
    return *info_;
}

const world::RenderRegion& ViewerRuntime::render_region() const {
    if (!render_region_.has_value()) {
        throw std::runtime_error(
            "Viewer runtime has no render Region");
    }
    return *render_region_;
}

const world::WorldModel& ViewerRuntime::world_model() const noexcept {
    return world_;
}

} // namespace ogl::viewer::app
