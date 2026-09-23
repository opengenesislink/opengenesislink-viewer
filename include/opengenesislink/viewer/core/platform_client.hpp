#pragma once

#include "opengenesislink/viewer/core/bootstrap_content.hpp"
#include "opengenesislink/viewer/core/http_transport.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ogl::viewer::core {

struct FriendRelation {
    std::string id;
    std::string other_user_id;
    std::string status;
    std::string requested_by;
    std::int64_t created_unix = 0;
    std::int64_t updated_unix = 0;
};

struct DirectMessage {
    std::string id;
    std::string sender_id;
    std::string recipient_id;
    std::string direction;
    std::string text;
    std::int64_t sent_unix = 0;
    std::int64_t read_unix = 0;
};

struct MessageList {
    std::size_t unread = 0U;
    std::vector<DirectMessage> messages;
};

struct SocialPolicy {
    std::string target_user_id;
    bool blocked = false;
    bool muted = false;
    std::int64_t updated_unix = 0;
};

struct PresenceInfo {
    std::string user_id;
    std::string display_name;
    std::string region_id;
    std::string node_id;
    std::uint64_t entity_id = 0U;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    std::int64_t updated_unix = 0;
};

struct NotificationInfo {
    std::string id;
    std::string type;
    std::string title;
    std::string body;
    std::string target;
    std::int64_t created_unix = 0;
    std::int64_t read_unix = 0;
};

struct NotificationList {
    std::size_t unread = 0U;
    std::vector<NotificationInfo> notifications;
};

struct GroupInfo {
    std::string id;
    std::string name;
    std::string founder_user_id;
    std::int64_t created_unix = 0;
};

struct GroupMember {
    std::string user_id;
    std::string role;
    std::uint64_t powers = 0U;
    std::int64_t joined_unix = 0;
};

struct GroupInvite {
    std::string id;
    std::string group_id;
    std::string invited_user_id;
    std::string invited_by_user_id;
    std::string role;
    std::string state;
    std::int64_t created_unix = 0;
    std::int64_t expires_unix = 0;
    std::int64_t resolved_unix = 0;
};

struct GroupPost {
    std::string id;
    std::string group_id;
    std::string sender_id;
    std::string kind;
    std::string title;
    std::string text;
    std::int64_t sent_unix = 0;
};

struct ParcelInfo {
    std::string id;
    std::string region_id;
    std::string name;
    std::string owner_user_id;
    std::string group_id;
    std::uint16_t x1 = 0U;
    std::uint16_t y1 = 0U;
    std::uint16_t x2 = 0U;
    std::uint16_t y2 = 0U;
    bool public_entry = false;
    bool public_build = false;
    bool group_build = false;
    bool group_terraform = false;
};

struct ParcelAccessEntry {
    std::string user_id;
    bool allowed = false;
    std::int64_t updated_unix = 0;
};

struct TravelSession {
    std::string user_id;
    std::string display_name;
    std::string region_id;
    std::string region_name;
    std::string scene_endpoint;
    std::string scene_ticket;
    std::vector<std::string> capabilities;
    std::string handoff_from;
    std::string crossing_id;
    std::vector<std::string> groups;
    SpawnPoint spawn;
    std::int64_t expires_unix = 0;
};

struct CrossingInfo {
    std::string id;
    std::string user_id;
    std::string from_region;
    std::string to_region;
    std::string state;
    std::string reservation_token;
    std::string rollback_reason;
    std::int64_t created_unix = 0;
    std::int64_t expires_unix = 0;
    std::int64_t reserved_unix = 0;
    std::int64_t completed_unix = 0;
    std::int64_t rolled_back_unix = 0;
};

class PlatformClient {
public:
    explicit PlatformClient(HttpTransport& transport);

    [[nodiscard]] AvatarAppearanceSnapshot appearance(
        const std::string& base_url,
        const std::string& bearer_token) const;

    [[nodiscard]] AvatarAppearanceSnapshot set_wearable(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view slot,
        std::string_view item_id,
        std::string_view asset_id) const;

    [[nodiscard]] AvatarAppearanceSnapshot remove_wearable(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view slot) const;

    [[nodiscard]] AvatarAppearanceSnapshot attach(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view point,
        std::string_view item_id,
        std::string_view asset_id) const;

    [[nodiscard]] AvatarAppearanceSnapshot detach(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view point,
        std::string_view item_id = {}) const;

    [[nodiscard]] InventorySnapshot inventory(
        const std::string& base_url,
        const std::string& bearer_token) const;

    [[nodiscard]] InventoryFolderSnapshot create_folder(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view parent_id,
        std::string_view name) const;

    [[nodiscard]] InventoryItemSnapshot create_item(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view parent_id,
        std::string_view asset_id,
        std::string_view name) const;

    [[nodiscard]] std::vector<PresenceInfo> presences(
        const std::string& base_url,
        const std::string& bearer_token) const;

    [[nodiscard]] std::vector<FriendRelation> friends(
        const std::string& base_url,
        const std::string& bearer_token) const;

    void request_friend(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view user_id) const;

    void accept_friend(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view user_id) const;

    void remove_friend(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view user_id) const;

    [[nodiscard]] MessageList messages(
        const std::string& base_url,
        const std::string& bearer_token) const;

    void send_message(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view recipient_id,
        std::string_view text) const;

    void mark_message_read(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view message_id) const;

    [[nodiscard]] std::vector<SocialPolicy> social_policies(
        const std::string& base_url,
        const std::string& bearer_token) const;

    void set_blocked(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view user_id,
        bool blocked) const;

    void set_muted(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view user_id,
        bool muted) const;

    [[nodiscard]] NotificationList notifications(
        const std::string& base_url,
        const std::string& bearer_token) const;

    void mark_notification_read(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view notification_id) const;

    [[nodiscard]] std::vector<GroupInfo> groups(
        const std::string& base_url,
        const std::string& bearer_token) const;

    [[nodiscard]] std::string create_group(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view name) const;

    [[nodiscard]] std::vector<GroupMember> group_members(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view group_id) const;

    void add_group_member(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view group_id,
        std::string_view user_id,
        std::string_view role = "member") const;

    void set_group_role(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view group_id,
        std::string_view user_id,
        std::string_view role) const;

    void remove_group_member(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view group_id,
        std::string_view user_id) const;

    [[nodiscard]] std::vector<GroupInvite> group_invites(
        const std::string& base_url,
        const std::string& bearer_token) const;

    void invite_group_member(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view group_id,
        std::string_view user_id,
        std::string_view role = "member",
        std::int64_t lifetime_seconds = 604800) const;

    void accept_group_invite(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view invite_id) const;

    void revoke_group_invite(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view invite_id) const;

    [[nodiscard]] std::vector<GroupPost> group_channel(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view group_id) const;

    void send_group_post(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view group_id,
        std::string_view kind,
        std::string_view title,
        std::string_view text) const;

    [[nodiscard]] std::vector<ParcelInfo> region_parcels(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view region_id) const;

    [[nodiscard]] std::vector<ParcelInfo> owned_parcels(
        const std::string& base_url,
        const std::string& bearer_token) const;

    [[nodiscard]] std::string create_parcel(
        const std::string& base_url,
        const std::string& bearer_token,
        const ParcelInfo& parcel) const;

    void update_parcel_policy(
        const std::string& base_url,
        const std::string& bearer_token,
        const ParcelInfo& parcel) const;

    [[nodiscard]] std::vector<ParcelAccessEntry> parcel_access(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view parcel_id) const;

    void set_parcel_access(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view parcel_id,
        std::string_view user_id,
        bool allowed) const;

    void remove_parcel_access(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view parcel_id,
        std::string_view user_id) const;

    [[nodiscard]] TravelSession teleport(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view region_id,
        double x = 128.0,
        double y = 128.0,
        double z = 0.0) const;

    [[nodiscard]] TravelSession handoff(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view from_region,
        std::string_view to_region,
        double vx = 0.0,
        double vy = 0.0,
        double vz = 0.0,
        double rx = 0.0,
        double ry = 0.0,
        double rz = 0.0) const;

    [[nodiscard]] CrossingInfo reserve_handoff(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view crossing_id,
        std::string_view region_id) const;

    [[nodiscard]] CrossingInfo complete_handoff(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view crossing_id,
        std::string_view region_id,
        std::string_view reservation_token) const;

    [[nodiscard]] CrossingInfo rollback_handoff(
        const std::string& base_url,
        const std::string& bearer_token,
        std::string_view crossing_id,
        std::string_view reason = "viewer-rollback") const;

private:
    HttpTransport& transport_;
};

} // namespace ogl::viewer::core
