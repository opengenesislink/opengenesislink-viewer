#include "opengenesislink/viewer/app/viewer_runtime.hpp"

#include <algorithm>
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
        core_base_url_ = request.core_base_url;
        region_id_ = entry.bootstrap.region_id;
        spawn_x_ = request.spawn_x;
        spawn_y_ = request.spawn_y;
        spawn_z_ = request.spawn_z;

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
        if (!info.spawn.has_value()) {
            info.spawn = core::SpawnPoint{
                .x = startup.join.spawn_x,
                .y = startup.join.spawn_y,
                .z = startup.join.spawn_z,
            };
        }

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

ConnectionInfo ViewerRuntime::reconnect() {
    if (bearer_token_.empty() ||
        core_base_url_.empty() ||
        region_id_.empty() ||
        !world_.initialized()) {
        throw std::runtime_error(
            "Viewer runtime has no reconnect context");
    }

    const auto previous_sequence = world_.sequence();
    scene_.disconnect();

    core::ViewerBootstrapClient bootstrap_client(http_);
    const auto bootstrap = bootstrap_client.bootstrap(
        core_base_url_,
        bearer_token_,
        {
            .region = region_id_,
            .x = spawn_x_,
            .y = spawn_y_,
            .z = spawn_z_,
        });

    try {
        const auto startup = scene_.connect_and_enter(
            bootstrap.scene_endpoint,
            bootstrap.region_id,
            bootstrap.scene_ticket,
            previous_sequence);

        const auto synchronized =
            synchronizer_.apply(startup.initial_sync);
        if (!synchronized.applied || !world_.initialized()) {
            throw std::runtime_error(
                "Reconnected Scene state was not applied");
        }

        render_region_ = render_builder_.build(world_);

        if (!info_.has_value()) {
            throw std::runtime_error(
                "Viewer runtime lost connection metadata");
        }

        info_->region_id = bootstrap.region_id;
        info_->scene_endpoint = bootstrap.scene_endpoint;
        info_->scene_capabilities = bootstrap.capabilities;
        info_->spawn = bootstrap.spawn;
        if (!info_->spawn.has_value()) {
            info_->spawn = core::SpawnPoint{
                .x = startup.join.spawn_x,
                .y = startup.join.spawn_y,
                .z = startup.join.spawn_z,
            };
        }

        region_id_ = bootstrap.region_id;
        return *info_;
    } catch (...) {
        scene_.disconnect();
        throw;
    }
}

void ViewerRuntime::disconnect() noexcept {
    scene_.disconnect();
    world_.clear();
    render_region_.reset();
    info_.reset();
    core_base_url_.clear();
    region_id_.clear();
    spawn_x_ = 128.0;
    spawn_y_ = 128.0;
    spawn_z_ = 25.0;

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
