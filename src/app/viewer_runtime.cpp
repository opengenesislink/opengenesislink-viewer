#include "opengenesislink/viewer/app/viewer_runtime.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace ogl::viewer::app {
namespace {

bool contains_capability(
    const std::vector<std::string>& capabilities,
    const std::string& capability) {
    return std::find(
               capabilities.begin(),
               capabilities.end(),
               capability) != capabilities.end();
}

} // namespace

ViewerRuntime::ViewerRuntime()
    : http_{},
      core_entry_{http_},
      asset_http_{},
      asset_client_{asset_http_},
      platform_http_{},
      platform_client_{platform_http_},
      asset_cache_{},
      bootstrap_content_{},
      scene_{},
      world_{},
      synchronizer_{scene_.session(), world_},
      render_builder_{},
      terrain_refinement_{} {}

ViewerRuntime::~ViewerRuntime() {
    disconnect();
}

void ViewerRuntime::rebuild_render_region() {
    if (!world_.initialized()) {
        render_region_.reset();
        return;
    }

    std::optional<world::TerrainPatch> previous_patch;
    if (render_region_.has_value() &&
        render_region_->terrain_patch.has_value()) {
        previous_patch = render_region_->terrain_patch;
    }

    auto next = render_builder_.build(world_);
    const auto& region = world_.region();

    if (previous_patch.has_value() &&
        previous_patch->revision == region.terrain_revision) {
        next.terrain_patch = std::move(previous_patch);
    }

    if (!terrain_refinement_.configured() ||
        terrain_refinement_.revision() !=
            region.terrain_revision) {
        terrain_refinement_.reset(region);
        terrain_suspended_ = false;
    }

    render_region_ = std::move(next);
}

void ViewerRuntime::prepare_asset_prefetch(
    const core::BootstrapContent& content) {
    bootstrap_content_ = content;
    asset_queue_.clear();
    asset_dependency_count_ = 0U;
    asset_ready_count_ = 0U;
    asset_failed_count_ = 0U;
    asset_missing_metadata_count_ = 0U;
    asset_error_.clear();

    if (!bootstrap_content_.appearance.has_value()) {
        return;
    }

    const auto dependencies =
        core::appearance_asset_dependencies(
            *bootstrap_content_.appearance);
    asset_dependency_count_ =
        dependencies.size();

    for (const auto& asset_id : dependencies) {
        const auto* metadata =
            core::find_asset_metadata(
                bootstrap_content_,
                asset_id);
        if (metadata == nullptr) {
            ++asset_missing_metadata_count_;
            continue;
        }

        if (asset_cache_.find(*metadata) != nullptr) {
            ++asset_ready_count_;
        } else {
            asset_queue_.push_back(*metadata);
        }
    }

    if (asset_missing_metadata_count_ != 0U) {
        asset_error_ =
            std::to_string(
                asset_missing_metadata_count_) +
            " Appearance Asset dependencies are missing bootstrap metadata";
    }
}

ConnectionInfo ViewerRuntime::connect(
    const LoginRequest& request) {
    disconnect();

    auto entry = core_entry_.enter(request);

    try {
        scene::SceneStartupResult startup;
        {
            std::lock_guard lock(scene_io_mutex_);
            startup = scene_.connect_and_enter(
                entry.bootstrap.scene_endpoint,
                entry.bootstrap.region_id,
                entry.bootstrap.scene_ticket);
        }

        const auto synchronized =
            synchronizer_.apply(startup.initial_sync);
        if (!synchronized.applied || !world_.initialized()) {
            throw std::runtime_error(
                "Initial authoritative Scene state was not applied");
        }

        avatar_id_ = startup.join.avatar_id;
        next_client_sequence_ = 1U;
        bearer_token_ = entry.login.token;
        core_base_url_ = request.core_base_url;
        region_id_ = entry.bootstrap.region_id;
        spawn_x_ = request.spawn_x;
        spawn_y_ = request.spawn_y;
        spawn_z_ = request.spawn_z;
        can_reconcile_avatar_ =
            contains_capability(
                entry.bootstrap.capabilities,
                "scene.avatar.reconcile");
        prepare_asset_prefetch(
            entry.bootstrap.content);
        terrain_suspended_ = false;
        background_error_.clear();
        last_boundary_.clear();

        rebuild_render_region();

        ConnectionInfo info;
        info.server_version =
            entry.discovery.release.server_version;
        info.channel =
            entry.discovery.release.channel;
        info.user_id = entry.login.user.id;
        info.username = entry.login.user.username;
        info.display_name =
            entry.login.user.display_name;
        info.region_id =
            entry.bootstrap.region_id;
        info.scene_endpoint =
            entry.bootstrap.scene_endpoint;
        info.scene_capabilities =
            entry.bootstrap.capabilities;
        info.spawn = entry.bootstrap.spawn;
        info.avatar_id = avatar_id_;
        info.avatar_reconcile_supported =
            can_reconcile_avatar_;

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

    std::unique_lock lock(
        scene_io_mutex_,
        std::try_to_lock);
    if (!lock.owns_lock()) {
        return {};
    }

    const auto result =
        synchronizer_.poll(max_events);
    lock.unlock();

    if (result.applied) {
        rebuild_render_region();
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

    wait_for_background();
    queued_movement_.reset();

    const auto previous_sequence = world_.sequence();

    {
        std::lock_guard lock(scene_io_mutex_);
        scene_.disconnect();
    }

    core::ViewerBootstrapClient bootstrap_client(http_);
    const auto bootstrap =
        bootstrap_client.bootstrap(
            core_base_url_,
            bearer_token_,
            {
                .region = region_id_,
                .x = spawn_x_,
                .y = spawn_y_,
                .z = spawn_z_,
            });

    try {
        scene::SceneStartupResult startup;
        {
            std::lock_guard lock(scene_io_mutex_);
            startup = scene_.connect_and_enter(
                bootstrap.scene_endpoint,
                bootstrap.region_id,
                bootstrap.scene_ticket,
                previous_sequence);
        }

        const auto synchronized =
            synchronizer_.apply(
                startup.initial_sync);
        if (!synchronized.applied ||
            !world_.initialized()) {
            throw std::runtime_error(
                "Reconnected Scene state was not applied");
        }

        avatar_id_ = startup.join.avatar_id;
        can_reconcile_avatar_ =
            contains_capability(
                bootstrap.capabilities,
                "scene.avatar.reconcile");
        prepare_asset_prefetch(
            bootstrap.content);
        terrain_suspended_ = false;
        background_error_.clear();
        last_boundary_.clear();

        rebuild_render_region();

        if (!info_.has_value()) {
            throw std::runtime_error(
                "Viewer runtime lost connection metadata");
        }

        info_->region_id = bootstrap.region_id;
        info_->scene_endpoint =
            bootstrap.scene_endpoint;
        info_->scene_capabilities =
            bootstrap.capabilities;
        info_->spawn = bootstrap.spawn;
        info_->avatar_id = avatar_id_;
        info_->avatar_reconcile_supported =
            can_reconcile_avatar_;

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
        std::lock_guard lock(scene_io_mutex_);
        scene_.disconnect();
        throw;
    }
}

void ViewerRuntime::launch_asset_fetch() {
    if (asset_pending_ ||
        asset_queue_.empty() ||
        bearer_token_.empty() ||
        core_base_url_.empty()) {
        return;
    }

    const auto expected =
        asset_queue_.front();
    asset_queue_.pop_front();

    const auto base_url = core_base_url_;
    auto token = bearer_token_;
    asset_pending_ = true;

    asset_future_ = std::async(
        std::launch::async,
        [this,
         expected,
         base_url,
         token]() mutable {
            try {
                auto asset =
                    asset_client_.fetch(
                        base_url,
                        token,
                        expected.id);
                std::fill(
                    token.begin(),
                    token.end(),
                    '\0');
                token.clear();
                return AssetTaskResult{
                    .expected = expected,
                    .asset = std::move(asset),
                };
            } catch (...) {
                std::fill(
                    token.begin(),
                    token.end(),
                    '\0');
                token.clear();
                throw;
            }
        });
}

void ViewerRuntime::launch_terrain_sample() {
    if (terrain_pending_ ||
        terrain_suspended_ ||
        !terrain_refinement_.active() ||
        movement_pending_ ||
        queued_movement_.has_value() ||
        !world_.initialized()) {
        return;
    }

    const auto point =
        terrain_refinement_.next_sample();
    if (!point.has_value()) {
        return;
    }

    const auto& region = world_.region();
    const auto expected_revision =
        region.terrain_revision;
    const auto x =
        static_cast<double>(point->x) *
        region.terrain_cell_size;
    const auto y =
        static_cast<double>(point->y) *
        region.terrain_cell_size;

    terrain_pending_ = true;
    terrain_future_ = std::async(
        std::launch::async,
        [this,
         point = *point,
         expected_revision,
         x,
         y]() {
            std::lock_guard lock(scene_io_mutex_);
            auto sample =
                scene_.session()
                    .request_terrain_sample(x, y);
            return TerrainTaskResult{
                .point = point,
                .expected_revision =
                    expected_revision,
                .sample = sample,
            };
        });
}

void ViewerRuntime::launch_movement() {
    if (movement_pending_ ||
        !queued_movement_.has_value()) {
        return;
    }

    auto request = *queued_movement_;
    queued_movement_.reset();
    movement_pending_ = true;

    movement_future_ = std::async(
        std::launch::async,
        [this, request]() {
            std::lock_guard lock(scene_io_mutex_);
            return scene_.session()
                .reconcile_avatar(request);
        });
}

void ViewerRuntime::service_background() {
    using namespace std::chrono_literals;

    if (asset_pending_ &&
        asset_future_.valid() &&
        asset_future_.wait_for(0s) ==
            std::future_status::ready) {
        asset_pending_ = false;
        try {
            auto result =
                asset_future_.get();

            if (!result.expected.content_hash.empty() &&
                result.asset.metadata.content_hash !=
                    result.expected.content_hash) {
                throw std::runtime_error(
                    "Fetched Asset content hash differs from bootstrap metadata");
            }
            if (result.expected.size != 0U &&
                result.asset.metadata.size !=
                    result.expected.size) {
                throw std::runtime_error(
                    "Fetched Asset size differs from bootstrap metadata");
            }
            if (!asset_cache_.put(
                    std::move(result.asset))) {
                throw std::runtime_error(
                    "Fetched Asset exceeds Viewer cache limits");
            }

            ++asset_ready_count_;
            if (asset_queue_.empty() &&
                asset_failed_count_ == 0U &&
                asset_missing_metadata_count_ == 0U) {
                asset_error_.clear();
            }
        } catch (const std::exception& ex) {
            ++asset_failed_count_;
            asset_error_ = ex.what();
        }
    }

    if (movement_pending_ &&
        movement_future_.valid() &&
        movement_future_.wait_for(0s) ==
            std::future_status::ready) {
        movement_pending_ = false;
        try {
            const auto ack =
                movement_future_.get();

            if (world_.initialized() &&
                avatar_id_ != 0U) {
                world::Transform transform;
                const auto& entities =
                    world_.region().entities;
                if (const auto found =
                        entities.find(avatar_id_);
                    found != entities.end()) {
                    transform.scale =
                        found->second.transform.scale;
                }

                transform.position = {
                    ack.pose.x,
                    ack.pose.y,
                    ack.pose.z,
                };
                transform.rotation = {
                    ack.pose.rx,
                    ack.pose.ry,
                    ack.pose.rz,
                };

                (void)world_.apply_reconciled_avatar(
                    avatar_id_,
                    transform,
                    {
                        ack.velocity.x,
                        ack.velocity.y,
                        ack.velocity.z,
                    });
                rebuild_render_region();
            }

            last_boundary_ = ack.boundary;
            background_error_.clear();
        } catch (const std::exception& ex) {
            background_error_ = ex.what();
        }
    }

    if (terrain_pending_ &&
        terrain_future_.valid() &&
        terrain_future_.wait_for(0s) ==
            std::future_status::ready) {
        terrain_pending_ = false;
        try {
            const auto result =
                terrain_future_.get();

            if (world_.initialized() &&
                result.expected_revision ==
                    world_.region().terrain_revision &&
                result.sample.revision ==
                    result.expected_revision &&
                terrain_refinement_.revision() ==
                    result.expected_revision) {
                terrain_refinement_.submit(
                    result.point,
                    result.sample.height,
                    result.sample.revision);

                if (auto patch =
                        terrain_refinement_
                            .take_completed_patch();
                    patch.has_value()) {
                    if (render_region_.has_value() &&
                        patch->revision ==
                            world_.region()
                                .terrain_revision) {
                        render_region_->terrain_patch =
                            std::move(*patch);
                    }
                }
            }
        } catch (const std::exception& ex) {
            terrain_suspended_ = true;
            background_error_ = ex.what();
        }
    }

    launch_asset_fetch();

    if (queued_movement_.has_value() &&
        !movement_pending_) {
        launch_movement();
        return;
    }

    launch_terrain_sample();
}

bool ViewerRuntime::queue_avatar_control(
    const input::AvatarControlInput& input) {
    if (!connected() ||
        !can_reconcile_avatar_ ||
        avatar_id_ == 0U) {
        return false;
    }

    const auto& entities =
        world_.region().entities;
    const auto found =
        entities.find(avatar_id_);
    if (found == entities.end() ||
        found->second.kind !=
            world::EntityKind::avatar) {
        return false;
    }

    const auto sequence =
        next_client_sequence_++;
    if (next_client_sequence_ == 0U) {
        next_client_sequence_ = 1U;
    }

    queued_movement_ =
        input::make_avatar_control_request(
            sequence,
            found->second.transform,
            input);
    return true;
}

void ViewerRuntime::wait_for_background() noexcept {
    queued_movement_.reset();

    if (movement_future_.valid()) {
        try {
            (void)movement_future_.get();
        } catch (...) {
        }
    }
    movement_pending_ = false;

    if (terrain_future_.valid()) {
        try {
            (void)terrain_future_.get();
        } catch (...) {
        }
    }
    terrain_pending_ = false;

    if (asset_future_.valid()) {
        try {
            (void)asset_future_.get();
        } catch (...) {
        }
    }
    asset_pending_ = false;
}

void ViewerRuntime::disconnect() noexcept {
    wait_for_background();

    {
        std::lock_guard lock(scene_io_mutex_);
        scene_.disconnect();
    }

    world_.clear();
    terrain_refinement_ = {};
    terrain_suspended_ = false;
    asset_queue_.clear();
    asset_cache_.clear();
    bootstrap_content_ = {};
    asset_dependency_count_ = 0U;
    asset_ready_count_ = 0U;
    asset_failed_count_ = 0U;
    asset_missing_metadata_count_ = 0U;
    asset_error_.clear();
    render_region_.reset();
    info_.reset();
    core_base_url_.clear();
    region_id_.clear();

    spawn_x_ = 128.0;
    spawn_y_ = 128.0;
    spawn_z_ = 25.0;

    avatar_id_ = 0U;
    next_client_sequence_ = 1U;
    can_reconcile_avatar_ = false;
    last_boundary_.clear();
    background_error_.clear();

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

const ConnectionInfo&
ViewerRuntime::connection_info() const {
    if (!info_.has_value()) {
        throw std::runtime_error(
            "Viewer runtime has no active connection");
    }
    return *info_;
}

const world::RenderRegion&
ViewerRuntime::render_region() const {
    if (!render_region_.has_value()) {
        throw std::runtime_error(
            "Viewer runtime has no render Region");
    }
    return *render_region_;
}

const world::WorldModel&
ViewerRuntime::world_model() const noexcept {
    return world_;
}

std::uint64_t ViewerRuntime::avatar_id() const noexcept {
    return avatar_id_;
}

RuntimeBackgroundState
ViewerRuntime::background_state() const {
    RuntimeBackgroundState state;
    if (render_region_.has_value() &&
        render_region_->terrain_patch.has_value()) {
        state.terrain_resolution =
            render_region_->terrain_patch->columns;
    }
    state.terrain_refining =
        terrain_pending_ ||
        (!terrain_suspended_ &&
         terrain_refinement_.active());
    state.movement_pending =
        movement_pending_ ||
        queued_movement_.has_value();
    state.asset_dependencies =
        asset_dependency_count_;
    state.asset_cached =
        asset_ready_count_;
    state.asset_failed =
        asset_failed_count_;
    state.asset_missing_metadata =
        asset_missing_metadata_count_;
    state.asset_cache_bytes =
        asset_cache_.bytes();
    state.asset_prefetch_pending =
        asset_pending_ ||
        !asset_queue_.empty();

    if (bootstrap_content_.appearance.has_value()) {
        state.appearance_revision =
            bootstrap_content_.appearance->revision;
    }
    if (bootstrap_content_.inventory.has_value()) {
        state.inventory_folders =
            bootstrap_content_.inventory->folders.size();
        state.inventory_items =
            bootstrap_content_.inventory->items.size();
    }

    state.boundary = last_boundary_;
    state.last_error =
        !background_error_.empty()
            ? background_error_
            : asset_error_;
    return state;
}

const core::BootstrapContent&
ViewerRuntime::bootstrap_content() const noexcept {
    return bootstrap_content_;
}

const core::AssetBlob* ViewerRuntime::cached_asset(
    std::string_view asset_id) {
    if (const auto* metadata =
            core::find_asset_metadata(
                bootstrap_content_,
                asset_id);
        metadata != nullptr) {
        return asset_cache_.find(*metadata);
    }
    return asset_cache_.find_by_id(asset_id);
}

bool ViewerRuntime::has_scene_capability(
    const std::string& capability) const {
    return info_.has_value() &&
           contains_capability(
               info_->scene_capabilities,
               capability);
}

} // namespace ogl::viewer::app
