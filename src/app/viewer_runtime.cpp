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

std::string join_arguments(
    const std::vector<std::string>& arguments,
    std::size_t first = 0U) {
    std::string result;
    for (std::size_t index = first;
         index < arguments.size();
         ++index) {
        if (!result.empty()) {
            result += ' ';
        }
        result += arguments[index];
    }
    return result;
}

template <typename T>
T parse_unsigned_argument(
    std::string_view value,
    std::string_view label) {
    T result{};
    const auto [ptr, error] =
        std::from_chars(
            value.data(),
            value.data() + value.size(),
            result);
    if (error != std::errc{} ||
        ptr != value.data() + value.size()) {
        throw std::invalid_argument(
            "Invalid " + std::string(label));
    }
    return result;
}

double parse_double_argument(
    std::string_view value,
    std::string_view label) {
    double result = 0.0;
    const auto [ptr, error] =
        std::from_chars(
            value.data(),
            value.data() + value.size(),
            result);
    if (error != std::errc{} ||
        ptr != value.data() + value.size() ||
        !std::isfinite(result)) {
        throw std::invalid_argument(
            "Invalid " + std::string(label));
    }
    return result;
}

bool parse_toggle_argument(
    std::string_view value,
    std::string_view label) {
    if (value == "on" ||
        value == "allow" ||
        value == "1" ||
        value == "true") {
        return true;
    }
    if (value == "off" ||
        value == "deny" ||
        value == "0" ||
        value == "false") {
        return false;
    }
    throw std::invalid_argument(
        "Invalid " + std::string(label) +
        "; expected on/off or allow/deny");
}

scene::SceneTransform to_scene_transform(
    const world::Transform& transform) {
    return {
        .x = transform.position.x,
        .y = transform.position.y,
        .z = transform.position.z,
        .rx = transform.rotation.x,
        .ry = transform.rotation.y,
        .rz = transform.rotation.z,
        .sx = transform.scale.x,
        .sy = transform.scale.y,
        .sz = transform.scale.z,
    };
}

std::string summarize_social(
    const std::vector<core::PresenceInfo>& presences,
    const std::vector<core::FriendRelation>& friends,
    const core::MessageList& messages,
    const std::vector<core::SocialPolicy>& policies) {
    std::ostringstream out;
    out << "SOCIAL: "
        << friends.size() << " FRIENDS, "
        << messages.unread << " UNREAD DM, "
        << presences.size() << " PRESENCE, "
        << policies.size() << " POLICIES";
    for (const auto& relation : friends) {
        out << "\nFRIEND "
            << relation.other_user_id
            << " [" << relation.status << ']';
    }
    for (const auto& message : messages.messages) {
        out << "\nDM "
            << message.direction << ' '
            << message.sender_id
            << " -> "
            << message.recipient_id
            << ": " << message.text;
    }
    return out.str();
}

std::string summarize_groups(
    const std::vector<core::GroupInfo>& groups,
    const std::vector<core::GroupInvite>& invites) {
    std::ostringstream out;
    out << "GROUPS: "
        << groups.size()
        << " MEMBERSHIPS, "
        << invites.size()
        << " INVITES";
    for (const auto& group : groups) {
        out << "\nGROUP "
            << group.id
            << " | " << group.name;
    }
    for (const auto& invite : invites) {
        out << "\nINVITE "
            << invite.id
            << " | GROUP "
            << invite.group_id
            << " | " << invite.state;
    }
    return out.str();
}

std::string summarize_group_details(
    std::string_view group_id,
    const std::vector<core::GroupMember>& members,
    const std::vector<core::GroupPost>& posts) {
    std::ostringstream out;
    out << "GROUP " << group_id
        << ": " << members.size()
        << " MEMBERS, "
        << posts.size() << " POSTS";
    for (const auto& member : members) {
        out << "\nMEMBER "
            << member.user_id
            << " | " << member.role
            << " | POWERS "
            << member.powers;
    }
    for (const auto& post : posts) {
        out << "\n" << post.kind
            << " " << post.sender_id
            << ": " << post.text;
    }
    return out.str();
}

std::string summarize_notifications(
    const core::NotificationList& notifications) {
    std::ostringstream out;
    out << "NOTIFICATIONS: "
        << notifications.unread
        << " UNREAD / "
        << notifications.notifications.size()
        << " TOTAL";
    for (const auto& item :
         notifications.notifications) {
        out << "\n"
            << (item.read_unix == 0 ? "* " : "  ")
            << item.id << " | "
            << item.title << " | "
            << item.body;
    }
    return out.str();
}

std::string summarize_parcels(
    const std::vector<core::ParcelInfo>& parcels,
    std::string_view heading) {
    std::ostringstream out;
    out << heading << ": "
        << parcels.size();
    for (const auto& parcel : parcels) {
        out << "\nPARCEL "
            << parcel.id
            << " | " << parcel.name
            << " | [" << parcel.x1
            << ',' << parcel.y1
            << "]-[" << parcel.x2
            << ',' << parcel.y2
            << "] ENTRY="
            << (parcel.public_entry ? "PUBLIC" : "RESTRICTED")
            << " BUILD="
            << (parcel.public_build ? "PUBLIC" : "CONTROLLED");
    }
    return out.str();
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
        command_pending_ ||
        command_sync_requested_ ||
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

    if (command_pending_ &&
        command_future_.valid() &&
        command_future_.wait_for(0s) ==
            std::future_status::ready) {
        command_pending_ = false;
        try {
            auto result =
                command_future_.get();
            apply_command_result(
                std::move(result));
        } catch (const std::exception& ex) {
            command_result_ =
                std::string{"COMMAND FAILED: "} +
                ex.what();
        } catch (...) {
            command_result_ =
                "COMMAND FAILED";
        }
    }

    if (command_sync_requested_ &&
        connected()) {
        std::unique_lock lock(
            scene_io_mutex_,
            std::try_to_lock);
        if (lock.owns_lock()) {
            try {
                const auto frame =
                    scene_.session()
                        .request_sync(
                            world_.sequence(),
                            256U);
                lock.unlock();
                const auto result =
                    synchronizer_.apply(frame);
                if (result.applied) {
                    rebuild_render_region();
                }
                command_sync_requested_ = false;
            } catch (const std::exception& ex) {
                lock.unlock();
                command_result_ =
                    std::string{
                        "COMMAND APPLIED; SYNC FAILED: "} +
                    ex.what();
                command_sync_requested_ = false;
            }
        }
    }

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

bool ViewerRuntime::submit_command(
    std::string_view input) {
    if (!connected() ||
        command_pending_ ||
        input.empty()) {
        return false;
    }

    ConsoleCommand command;
    try {
        command = parse_console_command(input);
    } catch (const std::exception& ex) {
        command_result_ = ex.what();
        return false;
    }

    const auto base = core_base_url_;
    auto token = bearer_token_;
    const auto current_region = region_id_;
    const auto current_avatar_id = avatar_id_;
    const auto region_snapshot = world_.region();

    command_pending_ = true;
    command_result_ = "COMMAND RUNNING";

    command_future_ = std::async(
        std::launch::async,
        [this,
         command = std::move(command),
         base,
         token = std::move(token),
         current_region,
         current_avatar_id,
         region_snapshot]() mutable {
            const auto scrub_token = [&token]() {
                std::fill(
                    token.begin(),
                    token.end(),
                    '\0');
                token.clear();
            };

            const auto platform_lock =
                [this, &base, &token](
                    auto&& operation)
                    -> decltype(auto) {
                    std::lock_guard lock(
                        platform_io_mutex_);
                    return operation(
                        platform_client_,
                        base,
                        token);
                };

            const auto scene_lock =
                [this](auto&& operation)
                    -> decltype(auto) {
                    std::lock_guard lock(
                        scene_io_mutex_);
                    return operation(
                        scene_.session());
                };

            CommandTaskResult result;

            try {
                const auto& args =
                    command.arguments;

                switch (command.kind) {
                case ConsoleCommandKind::help:
                    result.output = console_help();
                    break;

                case ConsoleCommandKind::local_chat: {
                    const auto text =
                        join_arguments(args);
                    (void)scene_lock(
                        [&text](scene::SceneSession& session) {
                            return session.send_chat(text);
                        });
                    result.output =
                        "LOCAL CHAT SENT";
                    result.scene_mutated = true;
                    break;
                }

                case ConsoleCommandKind::direct_message: {
                    const auto text =
                        join_arguments(args, 1U);
                    platform_lock(
                        [&args, &text](
                            core::PlatformClient& client,
                            const std::string& url,
                            const std::string& auth) {
                            client.send_message(
                                url,
                                auth,
                                args.at(0),
                                text);
                        });
                    result.output =
                        "DIRECT MESSAGE SENT TO " +
                        args.at(0);
                    break;
                }

                case ConsoleCommandKind::social_refresh: {
                    platform_lock(
                        [&result](
                            core::PlatformClient& client,
                            const std::string& url,
                            const std::string& auth) {
                            const auto presences =
                                client.presences(
                                    url, auth);
                            const auto friends =
                                client.friends(
                                    url, auth);
                            const auto messages =
                                client.messages(
                                    url, auth);
                            const auto policies =
                                client.social_policies(
                                    url, auth);
                            result.output =
                                summarize_social(
                                    presences,
                                    friends,
                                    messages,
                                    policies);
                        });
                    break;
                }

                case ConsoleCommandKind::friend_request:
                    platform_lock(
                        [&args](
                            core::PlatformClient& client,
                            const std::string& url,
                            const std::string& auth) {
                            client.request_friend(
                                url, auth, args.at(0));
                        });
                    result.output =
                        "FRIEND REQUEST SENT TO " +
                        args.at(0);
                    break;

                case ConsoleCommandKind::friend_accept:
                    platform_lock(
                        [&args](
                            core::PlatformClient& client,
                            const std::string& url,
                            const std::string& auth) {
                            client.accept_friend(
                                url, auth, args.at(0));
                        });
                    result.output =
                        "FRIEND ACCEPTED: " +
                        args.at(0);
                    break;

                case ConsoleCommandKind::friend_remove:
                    platform_lock(
                        [&args](
                            core::PlatformClient& client,
                            const std::string& url,
                            const std::string& auth) {
                            client.remove_friend(
                                url, auth, args.at(0));
                        });
                    result.output =
                        "FRIEND REMOVED: " +
                        args.at(0);
                    break;

                case ConsoleCommandKind::block:
                case ConsoleCommandKind::mute: {
                    const auto enabled =
                        parse_toggle_argument(
                            args.at(1),
                            "social toggle");
                    platform_lock(
                        [&args, enabled, &command](
                            core::PlatformClient& client,
                            const std::string& url,
                            const std::string& auth) {
                            if (command.kind ==
                                ConsoleCommandKind::block) {
                                client.set_blocked(
                                    url,
                                    auth,
                                    args.at(0),
                                    enabled);
                            } else {
                                client.set_muted(
                                    url,
                                    auth,
                                    args.at(0),
                                    enabled);
                            }
                        });
                    result.output =
                        std::string{
                            command.kind ==
                                    ConsoleCommandKind::block
                                ? "BLOCK "
                                : "MUTE "} +
                        (enabled ? "ENABLED: " : "DISABLED: ") +
                        args.at(0);
                    break;
                }

                case ConsoleCommandKind::appearance_refresh:
                    result.appearance =
                        platform_lock(
                            [](
                                core::PlatformClient& client,
                                const std::string& url,
                                const std::string& auth) {
                                return client.appearance(
                                    url, auth);
                            });
                    result.output =
                        "APPEARANCE REFRESHED";
                    break;

                case ConsoleCommandKind::wearable_set:
                    result.appearance =
                        platform_lock(
                            [&args](
                                core::PlatformClient& client,
                                const std::string& url,
                                const std::string& auth) {
                                return client.set_wearable(
                                    url,
                                    auth,
                                    args.at(0),
                                    args.at(1),
                                    args.at(2));
                            });
                    result.output =
                        "WEARABLE UPDATED: " +
                        args.at(0);
                    break;

                case ConsoleCommandKind::wearable_remove:
                    result.appearance =
                        platform_lock(
                            [&args](
                                core::PlatformClient& client,
                                const std::string& url,
                                const std::string& auth) {
                                return client.remove_wearable(
                                    url,
                                    auth,
                                    args.at(0));
                            });
                    result.output =
                        "WEARABLE REMOVED: " +
                        args.at(0);
                    break;

                case ConsoleCommandKind::attachment_set:
                    result.appearance =
                        platform_lock(
                            [&args](
                                core::PlatformClient& client,
                                const std::string& url,
                                const std::string& auth) {
                                return client.attach(
                                    url,
                                    auth,
                                    args.at(0),
                                    args.at(1),
                                    args.at(2));
                            });
                    result.output =
                        "ATTACHMENT UPDATED: " +
                        args.at(0);
                    break;

                case ConsoleCommandKind::attachment_remove:
                    result.appearance =
                        platform_lock(
                            [&args](
                                core::PlatformClient& client,
                                const std::string& url,
                                const std::string& auth) {
                                return client.detach(
                                    url,
                                    auth,
                                    args.at(0),
                                    args.size() > 1U
                                        ? std::string_view{
                                              args.at(1)}
                                        : std::string_view{});
                            });
                    result.output =
                        "ATTACHMENT REMOVED: " +
                        args.at(0);
                    break;

                case ConsoleCommandKind::inventory_refresh:
                    result.inventory =
                        platform_lock(
                            [](
                                core::PlatformClient& client,
                                const std::string& url,
                                const std::string& auth) {
                                return client.inventory(
                                    url, auth);
                            });
                    result.output =
                        "INVENTORY REFRESHED";
                    break;

                case ConsoleCommandKind::inventory_folder_create: {
                    const auto parent =
                        args.at(0) == "-"
                            ? std::string_view{}
                            : std::string_view{
                                  args.at(0)};
                    const auto name =
                        join_arguments(args, 1U);
                    platform_lock(
                        [parent, &name](
                            core::PlatformClient& client,
                            const std::string& url,
                            const std::string& auth) {
                            (void)client.create_folder(
                                url,
                                auth,
                                parent,
                                name);
                        });
                    result.inventory =
                        platform_lock(
                            [](
                                core::PlatformClient& client,
                                const std::string& url,
                                const std::string& auth) {
                                return client.inventory(
                                    url, auth);
                            });
                    result.output =
                        "INVENTORY FOLDER CREATED: " +
                        name;
                    break;
                }

                case ConsoleCommandKind::inventory_item_create: {
                    const auto parent =
                        args.at(0) == "-"
                            ? std::string_view{}
                            : std::string_view{
                                  args.at(0)};
                    const auto name =
                        join_arguments(args, 2U);
                    platform_lock(
                        [parent, &args, &name](
                            core::PlatformClient& client,
                            const std::string& url,
                            const std::string& auth) {
                            (void)client.create_item(
                                url,
                                auth,
                                parent,
                                args.at(1),
                                name);
                        });
                    result.inventory =
                        platform_lock(
                            [](
                                core::PlatformClient& client,
                                const std::string& url,
                                const std::string& auth) {
                                return client.inventory(
                                    url, auth);
                            });
                    result.output =
                        "INVENTORY ITEM CREATED: " +
                        name;
                    break;
                }

                case ConsoleCommandKind::groups_refresh:
                    platform_lock(
                        [&result](
                            core::PlatformClient& client,
                            const std::string& url,
                            const std::string& auth) {
                            const auto groups =
                                client.groups(
                                    url, auth);
                            const auto invites =
                                client.group_invites(
                                    url, auth);
                            result.output =
                                summarize_groups(
                                    groups,
                                    invites);
                        });
                    break;

                case ConsoleCommandKind::group_create: {
                    const auto name =
                        join_arguments(args);
                    const auto id =
                        platform_lock(
                            [&name](
                                core::PlatformClient& client,
                                const std::string& url,
                                const std::string& auth) {
                                return client.create_group(
                                    url, auth, name);
                            });
                    result.output =
                        "GROUP CREATED: " +
                        id + " | " + name;
                    break;
                }

                case ConsoleCommandKind::group_details:
                    platform_lock(
                        [&args, &result](
                            core::PlatformClient& client,
                            const std::string& url,
                            const std::string& auth) {
                            const auto members =
                                client.group_members(
                                    url,
                                    auth,
                                    args.at(0));
                            const auto posts =
                                client.group_channel(
                                    url,
                                    auth,
                                    args.at(0));
                            result.output =
                                summarize_group_details(
                                    args.at(0),
                                    members,
                                    posts);
                        });
                    break;

                case ConsoleCommandKind::group_invite:
                    platform_lock(
                        [&args](
                            core::PlatformClient& client,
                            const std::string& url,
                            const std::string& auth) {
                            client.invite_group_member(
                                url,
                                auth,
                                args.at(0),
                                args.at(1),
                                args.size() > 2U
                                    ? std::string_view{
                                          args.at(2)}
                                    : std::string_view{
                                          "member"});
                        });
                    result.output =
                        "GROUP INVITE SENT";
                    break;

                case ConsoleCommandKind::group_invite_accept:
                    platform_lock(
                        [&args](
                            core::PlatformClient& client,
                            const std::string& url,
                            const std::string& auth) {
                            client.accept_group_invite(
                                url,
                                auth,
                                args.at(0));
                        });
                    result.output =
                        "GROUP INVITE ACCEPTED";
                    break;

                case ConsoleCommandKind::group_member_add:
                    platform_lock(
                        [&args](
                            core::PlatformClient& client,
                            const std::string& url,
                            const std::string& auth) {
                            client.add_group_member(
                                url,
                                auth,
                                args.at(0),
                                args.at(1),
                                args.size() > 2U
                                    ? std::string_view{
                                          args.at(2)}
                                    : std::string_view{
                                          "member"});
                        });
                    result.output =
                        "GROUP MEMBER ADDED";
                    break;

                case ConsoleCommandKind::group_role:
                    platform_lock(
                        [&args](
                            core::PlatformClient& client,
                            const std::string& url,
                            const std::string& auth) {
                            client.set_group_role(
                                url,
                                auth,
                                args.at(0),
                                args.at(1),
                                args.at(2));
                        });
                    result.output =
                        "GROUP ROLE UPDATED";
                    break;

                case ConsoleCommandKind::group_member_remove:
                    platform_lock(
                        [&args](
                            core::PlatformClient& client,
                            const std::string& url,
                            const std::string& auth) {
                            client.remove_group_member(
                                url,
                                auth,
                                args.at(0),
                                args.at(1));
                        });
                    result.output =
                        "GROUP MEMBER REMOVED";
                    break;

                case ConsoleCommandKind::group_post: {
                    const auto text =
                        join_arguments(args, 2U);
                    platform_lock(
                        [&args, &text](
                            core::PlatformClient& client,
                            const std::string& url,
                            const std::string& auth) {
                            client.send_group_post(
                                url,
                                auth,
                                args.at(0),
                                args.at(1),
                                {},
                                text);
                        });
                    result.output =
                        "GROUP POST SENT";
                    break;
                }

                case ConsoleCommandKind::notifications_refresh: {
                    const auto notifications =
                        platform_lock(
                            [](
                                core::PlatformClient& client,
                                const std::string& url,
                                const std::string& auth) {
                                return client.notifications(
                                    url, auth);
                            });
                    result.output =
                        summarize_notifications(
                            notifications);
                    break;
                }

                case ConsoleCommandKind::notification_read:
                    platform_lock(
                        [&args](
                            core::PlatformClient& client,
                            const std::string& url,
                            const std::string& auth) {
                            client.mark_notification_read(
                                url,
                                auth,
                                args.at(0));
                        });
                    result.output =
                        "NOTIFICATION MARKED READ";
                    break;

                case ConsoleCommandKind::parcels_refresh:
                    platform_lock(
                        [&result, &current_region](
                            core::PlatformClient& client,
                            const std::string& url,
                            const std::string& auth) {
                            result.output =
                                summarize_parcels(
                                    client.region_parcels(
                                        url,
                                        auth,
                                        current_region),
                                    "REGION PARCELS");
                        });
                    break;

                case ConsoleCommandKind::parcel_create: {
                    const auto x1 =
                        parse_unsigned_argument<
                            std::uint16_t>(
                            args.at(1),
                            "parcel x1");
                    const auto y1 =
                        parse_unsigned_argument<
                            std::uint16_t>(
                            args.at(2),
                            "parcel y1");
                    const auto x2 =
                        parse_unsigned_argument<
                            std::uint16_t>(
                            args.at(3),
                            "parcel x2");
                    const auto y2 =
                        parse_unsigned_argument<
                            std::uint16_t>(
                            args.at(4),
                            "parcel y2");
                    const core::ParcelInfo parcel{
                        .region_id = current_region,
                        .name = args.at(0),
                        .x1 = x1,
                        .y1 = y1,
                        .x2 = x2,
                        .y2 = y2,
                    };
                    const auto id =
                        platform_lock(
                            [&parcel](
                                core::PlatformClient& client,
                                const std::string& url,
                                const std::string& auth) {
                                return client.create_parcel(
                                    url,
                                    auth,
                                    parcel);
                            });
                    result.output =
                        "PARCEL CREATED: " + id;
                    break;
                }

                case ConsoleCommandKind::parcel_policy: {
                    core::ParcelInfo parcel;
                    parcel.id = args.at(0);
                    if (args.at(1) != "-") {
                        parcel.group_id = args.at(1);
                    }
                    parcel.public_entry =
                        parse_toggle_argument(
                            args.at(2),
                            "public_entry");
                    parcel.public_build =
                        parse_toggle_argument(
                            args.at(3),
                            "public_build");
                    parcel.group_build =
                        parse_toggle_argument(
                            args.at(4),
                            "group_build");
                    parcel.group_terraform =
                        args.size() > 5U
                            ? parse_toggle_argument(
                                  args.at(5),
                                  "group_terraform")
                            : false;
                    platform_lock(
                        [&parcel](
                            core::PlatformClient& client,
                            const std::string& url,
                            const std::string& auth) {
                            client.update_parcel_policy(
                                url,
                                auth,
                                parcel);
                        });
                    result.output =
                        "PARCEL POLICY UPDATED";
                    break;
                }

                case ConsoleCommandKind::parcel_access_list: {
                    const auto access =
                        platform_lock(
                            [&args](
                                core::PlatformClient& client,
                                const std::string& url,
                                const std::string& auth) {
                                return client.parcel_access(
                                    url,
                                    auth,
                                    args.at(0));
                            });
                    std::ostringstream out;
                    out << "PARCEL ACCESS "
                        << args.at(0)
                        << ": " << access.size();
                    for (const auto& entry : access) {
                        out << "\n"
                            << entry.user_id
                            << " = "
                            << (entry.allowed
                                    ? "ALLOW"
                                    : "DENY");
                    }
                    result.output = out.str();
                    break;
                }

                case ConsoleCommandKind::parcel_access: {
                    const auto allowed =
                        parse_toggle_argument(
                            args.at(2),
                            "parcel access");
                    platform_lock(
                        [&args, allowed](
                            core::PlatformClient& client,
                            const std::string& url,
                            const std::string& auth) {
                            client.set_parcel_access(
                                url,
                                auth,
                                args.at(0),
                                args.at(1),
                                allowed);
                        });
                    result.output =
                        "PARCEL ACCESS UPDATED";
                    break;
                }

                case ConsoleCommandKind::parcel_access_remove:
                    platform_lock(
                        [&args](
                            core::PlatformClient& client,
                            const std::string& url,
                            const std::string& auth) {
                            client.remove_parcel_access(
                                url,
                                auth,
                                args.at(0),
                                args.at(1));
                        });
                    result.output =
                        "PARCEL ACCESS ENTRY REMOVED";
                    break;

                case ConsoleCommandKind::teleport: {
                    if (args.size() != 1U &&
                        args.size() != 4U) {
                        throw std::invalid_argument(
                            "Usage: /tp <region-id> [x y z]");
                    }
                    const auto x =
                        args.size() == 4U
                            ? parse_double_argument(
                                  args.at(1), "teleport x")
                            : 128.0;
                    const auto y =
                        args.size() == 4U
                            ? parse_double_argument(
                                  args.at(2), "teleport y")
                            : 128.0;
                    const auto z =
                        args.size() == 4U
                            ? parse_double_argument(
                                  args.at(3), "teleport z")
                            : 0.0;
                    result.travel =
                        platform_lock(
                            [&args, x, y, z](
                                core::PlatformClient& client,
                                const std::string& url,
                                const std::string& auth) {
                                return client.teleport(
                                    url,
                                    auth,
                                    args.at(0),
                                    x,
                                    y,
                                    z);
                            });
                    result.handoff = false;
                    result.output =
                        "TELEPORT TICKET READY: " +
                        result.travel->region_id;
                    break;
                }

                case ConsoleCommandKind::handoff: {
                    double vx = 0.0;
                    double vy = 0.0;
                    double vz = 0.0;
                    double rx = 0.0;
                    double ry = 0.0;
                    double rz = 0.0;
                    if (const auto avatar =
                            region_snapshot.entities.find(
                                current_avatar_id);
                        avatar !=
                            region_snapshot.entities.end()) {
                        vx =
                            avatar->second.physics.velocity.x;
                        vy =
                            avatar->second.physics.velocity.y;
                        vz =
                            avatar->second.physics.velocity.z;
                        rx =
                            avatar->second.transform.rotation.x;
                        ry =
                            avatar->second.transform.rotation.y;
                        rz =
                            avatar->second.transform.rotation.z;
                    }

                    result.travel =
                        platform_lock(
                            [&args,
                             &current_region,
                             vx, vy, vz,
                             rx, ry, rz](
                                core::PlatformClient& client,
                                const std::string& url,
                                const std::string& auth) {
                                return client.handoff(
                                    url,
                                    auth,
                                    current_region,
                                    args.at(0),
                                    vx, vy, vz,
                                    rx, ry, rz);
                            });
                    result.handoff = true;
                    result.output =
                        "HANDOFF PREPARED: " +
                        result.travel->region_id;
                    break;
                }

                case ConsoleCommandKind::build_list: {
                    std::ostringstream out;
                    std::size_t objects = 0U;
                    for (const auto& [id, entity] :
                         region_snapshot.entities) {
                        if (entity.kind !=
                            world::EntityKind::object) {
                            continue;
                        }
                        ++objects;
                        out << "\n#" << id
                            << " | " << entity.name
                            << " | "
                            << entity.transform.position.x
                            << ","
                            << entity.transform.position.y
                            << ","
                            << entity.transform.position.z
                            << " | SCALE "
                            << entity.transform.scale.x
                            << ","
                            << entity.transform.scale.y
                            << ","
                            << entity.transform.scale.z;
                    }
                    result.output =
                        "OBJECTS: " +
                        std::to_string(objects) +
                        out.str();
                    break;
                }

                case ConsoleCommandKind::build_create: {
                    scene::ObjectCreateRequest create;
                    create.name = args.at(0);
                    create.transform.x =
                        parse_double_argument(
                            args.at(1), "object x");
                    create.transform.y =
                        parse_double_argument(
                            args.at(2), "object y");
                    create.transform.z =
                        parse_double_argument(
                            args.at(3), "object z");
                    if (args.size() > 4U) {
                        create.physical =
                            parse_toggle_argument(
                                args.at(4),
                                "physical flag");
                    }
                    const auto ack =
                        scene_lock(
                            [&create](
                                scene::SceneSession& session) {
                                return session.create_object(
                                    create);
                            });
                    result.output =
                        "OBJECT CREATED: " +
                        std::to_string(
                            ack.entity_id);
                    result.scene_mutated = true;
                    break;
                }

                case ConsoleCommandKind::build_delete: {
                    const auto id =
                        parse_unsigned_argument<
                            std::uint64_t>(
                            args.at(0),
                            "entity id");
                    (void)scene_lock(
                        [id](
                            scene::SceneSession& session) {
                            return session.delete_object(id);
                        });
                    result.output =
                        "OBJECT DELETED: " +
                        std::to_string(id);
                    result.scene_mutated = true;
                    break;
                }

                case ConsoleCommandKind::build_move:
                case ConsoleCommandKind::build_scale: {
                    const auto id =
                        parse_unsigned_argument<
                            std::uint64_t>(
                            args.at(0),
                            "entity id");
                    const auto found =
                        region_snapshot.entities.find(id);
                    if (found ==
                        region_snapshot.entities.end()) {
                        throw std::invalid_argument(
                            "Entity is not present in current WorldModel");
                    }
                    auto transform =
                        to_scene_transform(
                            found->second.transform);
                    if (command.kind ==
                        ConsoleCommandKind::build_move) {
                        transform.x =
                            parse_double_argument(
                                args.at(1), "object x");
                        transform.y =
                            parse_double_argument(
                                args.at(2), "object y");
                        transform.z =
                            parse_double_argument(
                                args.at(3), "object z");
                    } else {
                        transform.sx =
                            parse_double_argument(
                                args.at(1), "scale x");
                        transform.sy =
                            parse_double_argument(
                                args.at(2), "scale y");
                        transform.sz =
                            parse_double_argument(
                                args.at(3), "scale z");
                    }

                    (void)scene_lock(
                        [id, transform](
                            scene::SceneSession& session) {
                            return session.update_object(
                                id,
                                transform);
                        });
                    result.output =
                        "OBJECT TRANSFORM UPDATED: " +
                        std::to_string(id);
                    result.scene_mutated = true;
                    break;
                }

                case ConsoleCommandKind::build_text: {
                    const auto id =
                        parse_unsigned_argument<
                            std::uint64_t>(
                            args.at(0),
                            "entity id");
                    const auto text =
                        join_arguments(args, 1U);
                    (void)scene_lock(
                        [id, &text](
                            scene::SceneSession& session) {
                            return session.set_object_text(
                                id,
                                text);
                        });
                    result.output =
                        "OBJECT TEXT UPDATED";
                    result.scene_mutated = true;
                    break;
                }

                case ConsoleCommandKind::build_link:
                case ConsoleCommandKind::build_unlink: {
                    const auto root =
                        parse_unsigned_argument<
                            std::uint64_t>(
                            args.at(0),
                            "root id");
                    const auto child =
                        parse_unsigned_argument<
                            std::uint64_t>(
                            args.at(1),
                            "child id");
                    const auto unlink =
                        command.kind ==
                        ConsoleCommandKind::build_unlink;
                    (void)scene_lock(
                        [root, child, unlink](
                            scene::SceneSession& session) {
                            return session.link_object(
                                root,
                                child,
                                unlink);
                        });
                    result.output =
                        unlink
                            ? "OBJECT UNLINKED"
                            : "OBJECT LINKED";
                    result.scene_mutated = true;
                    break;
                }

                case ConsoleCommandKind::build_permissions: {
                    const auto id =
                        parse_unsigned_argument<
                            std::uint64_t>(
                            args.at(0),
                            "entity id");
                    scene::ObjectPermissionsRequest permissions;
                    permissions.entity_id = id;
                    if (args.at(1) != "-") {
                        permissions.group_id =
                            args.at(1);
                    }
                    permissions.group_permissions =
                        parse_unsigned_argument<
                            std::uint32_t>(
                            args.at(2),
                            "group permission mask");
                    permissions.everyone_permissions =
                        parse_unsigned_argument<
                            std::uint32_t>(
                            args.at(3),
                            "everyone permission mask");
                    (void)scene_lock(
                        [&permissions](
                            scene::SceneSession& session) {
                            return session
                                .update_object_permissions(
                                    permissions);
                        });
                    result.output =
                        "OBJECT PERMISSIONS UPDATED";
                    result.scene_mutated = true;
                    break;
                }

                case ConsoleCommandKind::build_motion: {
                    scene::ObjectMotionRequest motion;
                    motion.entity_id =
                        parse_unsigned_argument<
                            std::uint64_t>(
                            args.at(0),
                            "entity id");
                    motion.vx =
                        parse_double_argument(
                            args.at(1), "vx");
                    motion.vy =
                        parse_double_argument(
                            args.at(2), "vy");
                    motion.vz =
                        parse_double_argument(
                            args.at(3), "vz");
                    motion.avx =
                        parse_double_argument(
                            args.at(4), "avx");
                    motion.avy =
                        parse_double_argument(
                            args.at(5), "avy");
                    motion.avz =
                        parse_double_argument(
                            args.at(6), "avz");
                    (void)scene_lock(
                        [&motion](
                            scene::SceneSession& session) {
                            return session
                                .set_object_motion(
                                    motion);
                        });
                    result.output =
                        "OBJECT MOTION UPDATED";
                    result.scene_mutated = true;
                    break;
                }

                case ConsoleCommandKind::build_interact: {
                    const auto id =
                        parse_unsigned_argument<
                            std::uint64_t>(
                            args.at(0),
                            "entity id");
                    (void)scene_lock(
                        [id, &args](
                            scene::SceneSession& session) {
                            return session.interact_object(
                                id,
                                args.at(1));
                        });
                    result.output =
                        "OBJECT INTERACTION SENT";
                    break;
                }

                case ConsoleCommandKind::build_shape: {
                    const auto id =
                        parse_unsigned_argument<
                            std::uint64_t>(
                            args.at(0),
                            "entity id");
                    const auto fields =
                        "shape=" +
                        args.at(1) + "\n";
                    (void)scene_lock(
                        [id, &fields](
                            scene::SceneSession& session) {
                            return session.object_physics(
                                id,
                                "shape",
                                fields);
                        });
                    result.output =
                        "OBJECT PHYSICS SHAPE UPDATED";
                    result.scene_mutated = true;
                    break;
                }

                case ConsoleCommandKind::build_material: {
                    const auto id =
                        parse_unsigned_argument<
                            std::uint64_t>(
                            args.at(0),
                            "entity id");
                    const auto mass =
                        parse_double_argument(
                            args.at(1), "mass");
                    const auto restitution =
                        parse_double_argument(
                            args.at(2), "restitution");
                    const auto friction =
                        parse_double_argument(
                            args.at(3), "friction");
                    std::ostringstream fields;
                    fields << "mass=" << mass
                           << "\nrestitution="
                           << restitution
                           << "\nfriction="
                           << friction << '\n';
                    const auto encoded =
                        fields.str();
                    (void)scene_lock(
                        [id, &encoded](
                            scene::SceneSession& session) {
                            return session.object_physics(
                                id,
                                "material",
                                encoded);
                        });
                    result.output =
                        "OBJECT PHYSICS MATERIAL UPDATED";
                    result.scene_mutated = true;
                    break;
                }

                case ConsoleCommandKind::build_force: {
                    const auto id =
                        parse_unsigned_argument<
                            std::uint64_t>(
                            args.at(0),
                            "entity id");
                    const auto x =
                        parse_double_argument(
                            args.at(1), "force x");
                    const auto y =
                        parse_double_argument(
                            args.at(2), "force y");
                    const auto z =
                        parse_double_argument(
                            args.at(3), "force z");
                    std::ostringstream fields;
                    fields << "x=" << x
                           << "\ny=" << y
                           << "\nz=" << z
                           << '\n';
                    const auto encoded =
                        fields.str();
                    (void)scene_lock(
                        [id, &encoded](
                            scene::SceneSession& session) {
                            return session.object_physics(
                                id,
                                "force",
                                encoded);
                        });
                    result.output =
                        "OBJECT FORCE APPLIED";
                    result.scene_mutated = true;
                    break;
                }

                case ConsoleCommandKind::terrain_set: {
                    const auto grid_x =
                        parse_unsigned_argument<
                            std::size_t>(
                            args.at(0),
                            "terrain x");
                    const auto grid_y =
                        parse_unsigned_argument<
                            std::size_t>(
                            args.at(1),
                            "terrain y");
                    const auto height =
                        parse_double_argument(
                            args.at(2),
                            "terrain height");
                    (void)scene_lock(
                        [grid_x, grid_y, height](
                            scene::SceneSession& session) {
                            return session.set_terrain_height(
                                grid_x,
                                grid_y,
                                height);
                        });
                    result.output =
                        "TERRAIN HEIGHT UPDATED";
                    result.scene_mutated = true;
                    break;
                }

                case ConsoleCommandKind::refresh_all: {
                    platform_lock(
                        [&result, &current_region](
                            core::PlatformClient& client,
                            const std::string& url,
                            const std::string& auth) {
                            result.appearance =
                                client.appearance(
                                    url, auth);
                            result.inventory =
                                client.inventory(
                                    url, auth);
                            const auto friends =
                                client.friends(
                                    url, auth);
                            const auto messages =
                                client.messages(
                                    url, auth);
                            const auto groups =
                                client.groups(
                                    url, auth);
                            const auto notifications =
                                client.notifications(
                                    url, auth);
                            const auto parcels =
                                client.region_parcels(
                                    url,
                                    auth,
                                    current_region);

                            std::ostringstream out;
                            out << "REFRESHED: "
                                << friends.size()
                                << " FRIENDS, "
                                << messages.unread
                                << " UNREAD DM, "
                                << groups.size()
                                << " GROUPS, "
                                << notifications.unread
                                << " UNREAD NOTIFICATIONS, "
                                << parcels.size()
                                << " PARCELS";
                            result.output = out.str();
                        });
                    break;
                }
                }

                scrub_token();
                return result;
            } catch (...) {
                scrub_token();
                throw;
            }
        });

    return true;
}

ViewerCommandState ViewerRuntime::command_state() const {
    return {
        .pending = command_pending_,
        .last_result = command_result_,
    };
}

void ViewerRuntime::apply_command_result(
    CommandTaskResult result) {
    if (result.appearance.has_value()) {
        bootstrap_content_.appearance =
            std::move(result.appearance);
        prepare_asset_prefetch(
            bootstrap_content_);
    }
    if (result.inventory.has_value()) {
        bootstrap_content_.inventory =
            std::move(result.inventory);
    }

    if (result.travel.has_value()) {
        apply_travel(
            *result.travel,
            result.handoff);
    }

    if (result.scene_mutated) {
        command_sync_requested_ = true;
    }

    command_result_ =
        result.output.empty()
            ? std::string{"COMMAND COMPLETED"}
            : std::move(result.output);
}

void ViewerRuntime::apply_travel(
    const core::TravelSession& travel,
    bool handoff) {
    if (travel.region_id.empty() ||
        travel.scene_endpoint.empty() ||
        travel.scene_ticket.empty()) {
        throw std::runtime_error(
            "Travel session is incomplete");
    }

    const auto source_region = region_id_;
    const auto source_x = spawn_x_;
    const auto source_y = spawn_y_;
    const auto source_z = spawn_z_;

    queued_movement_.reset();

    if (movement_future_.valid()) {
        (void)movement_future_.get();
    }
    movement_pending_ = false;

    if (terrain_future_.valid()) {
        (void)terrain_future_.get();
    }
    terrain_pending_ = false;

    std::optional<core::CrossingInfo> reservation;

    try {
        scene::SceneStartupResult startup;
        {
            std::lock_guard lock(scene_io_mutex_);
            scene_.disconnect();
            startup = scene_.connect_and_enter(
                travel.scene_endpoint,
                travel.region_id,
                travel.scene_ticket,
                0U);
        }

        if (handoff) {
            if (travel.crossing_id.empty()) {
                throw std::runtime_error(
                    "Handoff response is missing crossing id");
            }

            reservation =
                [&]() {
                    std::lock_guard lock(
                        platform_io_mutex_);
                    return platform_client_
                        .reserve_handoff(
                            core_base_url_,
                            bearer_token_,
                            travel.crossing_id,
                            travel.region_id);
                }();

            if (reservation->reservation_token.empty()) {
                throw std::runtime_error(
                    "Handoff reservation has no token");
            }

            {
                std::lock_guard lock(
                    platform_io_mutex_);
                (void)platform_client_
                    .complete_handoff(
                        core_base_url_,
                        bearer_token_,
                        travel.crossing_id,
                        travel.region_id,
                        reservation
                            ->reservation_token);
            }
        }

        const auto synchronized =
            synchronizer_.apply(
                startup.initial_sync);
        if (!synchronized.applied ||
            !world_.initialized()) {
            throw std::runtime_error(
                "Destination Scene state was not applied");
        }

        region_id_ = travel.region_id;
        spawn_x_ = travel.spawn.x;
        spawn_y_ = travel.spawn.y;
        spawn_z_ = travel.spawn.z;
        avatar_id_ = startup.join.avatar_id;
        next_client_sequence_ = 1U;
        can_reconcile_avatar_ =
            contains_capability(
                travel.capabilities,
                "scene.avatar.reconcile");

        terrain_refinement_ = {};
        terrain_suspended_ = false;
        last_boundary_.clear();
        background_error_.clear();

        if (!info_.has_value()) {
            throw std::runtime_error(
                "Viewer runtime lost connection metadata");
        }

        info_->region_id =
            travel.region_id;
        info_->scene_endpoint =
            travel.scene_endpoint;
        info_->scene_capabilities =
            travel.capabilities;
        info_->spawn = travel.spawn;
        info_->avatar_id = avatar_id_;
        info_->avatar_reconcile_supported =
            can_reconcile_avatar_;

        rebuild_render_region();
        command_result_ =
            std::string{
                handoff
                    ? "HANDOFF COMPLETE: "
                    : "TELEPORT COMPLETE: "} +
            travel.region_id;
    } catch (...) {
        if (handoff &&
            !travel.crossing_id.empty()) {
            try {
                std::lock_guard lock(
                    platform_io_mutex_);
                (void)platform_client_
                    .rollback_handoff(
                        core_base_url_,
                        bearer_token_,
                        travel.crossing_id,
                        "viewer-destination-failure");
            } catch (...) {
            }
        }

        region_id_ = source_region;
        spawn_x_ = source_x;
        spawn_y_ = source_y;
        spawn_z_ = source_z;
        recover_source_region(
            "travel-failed");
        throw;
    }
}

void ViewerRuntime::recover_source_region(
    std::string_view reason) noexcept {
    try {
        core::ViewerBootstrapClient bootstrap_client(
            http_);
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

        scene::SceneStartupResult startup;
        {
            std::lock_guard lock(
                scene_io_mutex_);
            scene_.disconnect();
            startup =
                scene_.connect_and_enter(
                    bootstrap.scene_endpoint,
                    bootstrap.region_id,
                    bootstrap.scene_ticket,
                    0U);
        }

        world_.clear();
        const auto synchronized =
            synchronizer_.apply(
                startup.initial_sync);
        if (!synchronized.applied ||
            !world_.initialized()) {
            throw std::runtime_error(
                "Source Region recovery sync failed");
        }

        avatar_id_ =
            startup.join.avatar_id;
        next_client_sequence_ = 1U;
        can_reconcile_avatar_ =
            contains_capability(
                bootstrap.capabilities,
                "scene.avatar.reconcile");
        prepare_asset_prefetch(
            bootstrap.content);
        terrain_refinement_ = {};
        terrain_suspended_ = false;

        if (info_.has_value()) {
            info_->region_id =
                bootstrap.region_id;
            info_->scene_endpoint =
                bootstrap.scene_endpoint;
            info_->scene_capabilities =
                bootstrap.capabilities;
            info_->spawn = bootstrap.spawn;
            info_->avatar_id =
                avatar_id_;
            info_->avatar_reconcile_supported =
                can_reconcile_avatar_;
        }

        rebuild_render_region();
        background_error_ =
            "Recovered source Region after " +
            std::string(reason);
    } catch (const std::exception& ex) {
        background_error_ =
            "Source Region recovery failed: " +
            std::string(ex.what());
    } catch (...) {
        background_error_ =
            "Source Region recovery failed";
    }
}

void ViewerRuntime::wait_for_background() noexcept {
    queued_movement_.reset();

    if (command_future_.valid()) {
        try {
            (void)command_future_.get();
        } catch (...) {
        }
    }
    command_pending_ = false;
    command_sync_requested_ = false;

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
    command_result_.clear();
    command_pending_ = false;
    command_sync_requested_ = false;

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
