#include "opengenesislink/viewer/core/platform_client.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace ogl::viewer::core {
namespace {

using Json = nlohmann::json;

std::string base_url(const std::string& base) {
    if (base.empty()) {
        throw std::invalid_argument(
            "Core base URL must not be empty");
    }
    return base.back() == '/'
        ? base.substr(0U, base.size() - 1U)
        : base;
}

std::string path_component(
    std::string_view value,
    std::string_view label) {
    if (value.empty()) {
        throw std::invalid_argument(
            std::string(label) + " must not be empty");
    }

    for (const unsigned char character : value) {
        if (!std::isalnum(character) &&
            character != '-' &&
            character != '_' &&
            character != '.') {
            throw std::invalid_argument(
                std::string(label) +
                " contains an unsafe path character");
        }
    }
    return std::string(value);
}

std::string required_text(
    std::string_view value,
    std::string_view label,
    std::size_t max_bytes = 4096U) {
    if (value.empty() || value.size() > max_bytes) {
        throw std::invalid_argument(
            std::string(label) +
            " has an invalid length");
    }
    return std::string(value);
}

std::string string_value(
    const Json& value,
    const char* key) {
    if (!value.is_object()) return {};
    const auto found = value.find(key);
    return found != value.end() &&
           found->is_string()
        ? found->get<std::string>()
        : std::string{};
}

std::int64_t i64_value(
    const Json& value,
    const char* key) {
    if (!value.is_object()) return 0;
    const auto found = value.find(key);
    return found != value.end() &&
           found->is_number_integer()
        ? found->get<std::int64_t>()
        : 0;
}

std::uint64_t u64_value(
    const Json& value,
    const char* key) {
    if (!value.is_object()) return 0U;
    const auto found = value.find(key);
    if (found == value.end()) return 0U;
    if (found->is_number_unsigned()) {
        return found->get<std::uint64_t>();
    }
    if (found->is_number_integer()) {
        const auto signed_value =
            found->get<std::int64_t>();
        return signed_value >= 0
            ? static_cast<std::uint64_t>(
                  signed_value)
            : 0U;
    }
    return 0U;
}

bool bool_value(
    const Json& value,
    const char* key,
    bool fallback = false) {
    if (!value.is_object()) return fallback;
    const auto found = value.find(key);
    return found != value.end() &&
           found->is_boolean()
        ? found->get<bool>()
        : fallback;
}

double double_value(
    const Json& value,
    const char* key,
    double fallback = 0.0) {
    if (!value.is_object()) return fallback;
    const auto found = value.find(key);
    if (found == value.end() ||
        !found->is_number()) {
        return fallback;
    }
    const auto result = found->get<double>();
    return std::isfinite(result)
        ? result
        : fallback;
}

std::vector<std::string> csv_list(
    std::string_view csv) {
    std::vector<std::string> result;
    std::size_t start = 0U;

    while (start <= csv.size()) {
        const auto separator =
            csv.find(',', start);
        auto part = csv.substr(
            start,
            separator == std::string_view::npos
                ? std::string_view::npos
                : separator - start);
        if (!part.empty()) {
            result.emplace_back(part);
        }
        if (separator == std::string_view::npos) {
            break;
        }
        start = separator + 1U;
    }
    return result;
}

Json parse_json(
    const HttpResponse& response,
    std::string_view operation) {
    if (response.status < 200 ||
        response.status >= 300) {
        std::string reason;
        try {
            const auto error =
                Json::parse(response.body);
            reason = string_value(error, "error");
        } catch (...) {
        }

        throw std::runtime_error(
            std::string(operation) +
            " failed with HTTP " +
            std::to_string(response.status) +
            (reason.empty()
                 ? std::string{}
                 : ": " + reason));
    }

    if (response.body.empty()) {
        return Json::object();
    }

    try {
        return Json::parse(response.body);
    } catch (const Json::exception& ex) {
        throw std::runtime_error(
            std::string(operation) +
            " returned invalid JSON: " +
            ex.what());
    }
}

Json request(
    HttpTransport& transport,
    const std::string& base,
    const std::string& bearer_token,
    std::string method,
    std::string path,
    const Json& body = Json{}) {
    if (bearer_token.empty()) {
        throw std::invalid_argument(
            "Bearer token must not be empty");
    }

    HttpRequest http;
    http.method = std::move(method);
    http.url = base_url(base) + path;
    http.headers.emplace(
        "Authorization",
        "Bearer " + bearer_token);
    http.max_response_bytes =
        8U * 1024U * 1024U;

    if (!body.is_null() &&
        !body.empty()) {
        http.headers.emplace(
            "Content-Type",
            "application/json");
        http.body = body.dump();
    }

    return parse_json(
        transport.perform(http),
        path);
}

const Json& array_field(
    const Json& doc,
    const char* key) {
    const auto found = doc.find(key);
    if (found == doc.end() ||
        !found->is_array()) {
        throw std::runtime_error(
            std::string{"Platform response is missing array: "} +
            key);
    }
    return *found;
}

std::vector<FriendRelation> parse_friends(
    const Json& array) {
    std::vector<FriendRelation> result;
    result.reserve(array.size());
    for (const auto& item : array) {
        if (!item.is_object()) continue;
        result.push_back({
            .id = string_value(item, "id"),
            .other_user_id =
                string_value(item, "other_user_id"),
            .status =
                string_value(item, "status"),
            .requested_by =
                string_value(item, "requested_by"),
            .created_unix =
                i64_value(item, "created_unix"),
            .updated_unix =
                i64_value(item, "updated_unix"),
        });
    }
    return result;
}

std::vector<DirectMessage> parse_messages(
    const Json& array) {
    std::vector<DirectMessage> result;
    result.reserve(array.size());
    for (const auto& item : array) {
        if (!item.is_object()) continue;
        result.push_back({
            .id = string_value(item, "id"),
            .sender_id =
                string_value(item, "sender_id"),
            .recipient_id =
                string_value(item, "recipient_id"),
            .direction =
                string_value(item, "direction"),
            .text = string_value(item, "text"),
            .sent_unix =
                i64_value(item, "sent_unix"),
            .read_unix =
                i64_value(item, "read_unix"),
        });
    }
    return result;
}

std::vector<SocialPolicy> parse_policies(
    const Json& array) {
    std::vector<SocialPolicy> result;
    result.reserve(array.size());
    for (const auto& item : array) {
        if (!item.is_object()) continue;
        result.push_back({
            .target_user_id =
                string_value(item, "target_user_id"),
            .blocked =
                bool_value(item, "blocked"),
            .muted =
                bool_value(item, "muted"),
            .updated_unix =
                i64_value(item, "updated_unix"),
        });
    }
    return result;
}

std::vector<PresenceInfo> parse_presences(
    const Json& array) {
    std::vector<PresenceInfo> result;
    result.reserve(array.size());
    for (const auto& item : array) {
        if (!item.is_object()) continue;
        result.push_back({
            .user_id =
                string_value(item, "user_id"),
            .display_name =
                string_value(item, "display_name"),
            .region_id =
                string_value(item, "region_id"),
            .node_id =
                string_value(item, "node_id"),
            .entity_id =
                u64_value(item, "entity_id"),
            .x = double_value(item, "x"),
            .y = double_value(item, "y"),
            .z = double_value(item, "z"),
            .updated_unix =
                i64_value(item, "updated_unix"),
        });
    }
    return result;
}

std::vector<NotificationInfo> parse_notifications(
    const Json& array) {
    std::vector<NotificationInfo> result;
    result.reserve(array.size());
    for (const auto& item : array) {
        if (!item.is_object()) continue;
        result.push_back({
            .id = string_value(item, "id"),
            .type = string_value(item, "type"),
            .title = string_value(item, "title"),
            .body = string_value(item, "body"),
            .target =
                string_value(item, "target"),
            .created_unix =
                i64_value(item, "created_unix"),
            .read_unix =
                i64_value(item, "read_unix"),
        });
    }
    return result;
}

std::vector<GroupInfo> parse_groups(
    const Json& array) {
    std::vector<GroupInfo> result;
    result.reserve(array.size());
    for (const auto& item : array) {
        if (!item.is_object()) continue;
        result.push_back({
            .id = string_value(item, "id"),
            .name = string_value(item, "name"),
            .founder_user_id =
                string_value(item, "founder_user_id"),
            .created_unix =
                i64_value(item, "created_unix"),
        });
    }
    return result;
}

std::vector<GroupMember> parse_members(
    const Json& array) {
    std::vector<GroupMember> result;
    result.reserve(array.size());
    for (const auto& item : array) {
        if (!item.is_object()) continue;
        result.push_back({
            .user_id =
                string_value(item, "user_id"),
            .role = string_value(item, "role"),
            .powers =
                u64_value(item, "powers"),
            .joined_unix =
                i64_value(item, "joined_unix"),
        });
    }
    return result;
}

std::vector<GroupInvite> parse_invites(
    const Json& array) {
    std::vector<GroupInvite> result;
    result.reserve(array.size());
    for (const auto& item : array) {
        if (!item.is_object()) continue;
        result.push_back({
            .id = string_value(item, "id"),
            .group_id =
                string_value(item, "group_id"),
            .invited_user_id =
                string_value(item, "invited_user_id"),
            .invited_by_user_id =
                string_value(
                    item,
                    "invited_by_user_id"),
            .role = string_value(item, "role"),
            .state = string_value(item, "state"),
            .created_unix =
                i64_value(item, "created_unix"),
            .expires_unix =
                i64_value(item, "expires_unix"),
            .resolved_unix =
                i64_value(item, "resolved_unix"),
        });
    }
    return result;
}

std::vector<GroupPost> parse_posts(
    const Json& array) {
    std::vector<GroupPost> result;
    result.reserve(array.size());
    for (const auto& item : array) {
        if (!item.is_object()) continue;
        result.push_back({
            .id = string_value(item, "id"),
            .group_id =
                string_value(item, "group_id"),
            .sender_id =
                string_value(item, "sender_id"),
            .kind = string_value(item, "kind"),
            .title = string_value(item, "title"),
            .text = string_value(item, "text"),
            .sent_unix =
                i64_value(item, "sent_unix"),
        });
    }
    return result;
}

ParcelInfo parse_parcel(const Json& item) {
    const auto u16 =
        [&item](const char* key) {
            const auto value = u64_value(item, key);
            if (value >
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::uint16_t>::max())) {
                throw std::runtime_error(
                    std::string{"Parcel coordinate out of range: "} +
                    key);
            }
            return static_cast<std::uint16_t>(value);
        };

    return {
        .id = string_value(item, "id"),
        .region_id =
            string_value(item, "region_id"),
        .name = string_value(item, "name"),
        .owner_user_id =
            string_value(item, "owner_user_id"),
        .group_id =
            string_value(item, "group_id"),
        .x1 = u16("x1"),
        .y1 = u16("y1"),
        .x2 = u16("x2"),
        .y2 = u16("y2"),
        .public_entry =
            bool_value(item, "public_entry"),
        .public_build =
            bool_value(item, "public_build"),
        .group_build =
            bool_value(item, "group_build"),
        .group_terraform =
            bool_value(item, "group_terraform"),
    };
}

std::vector<ParcelInfo> parse_parcels(
    const Json& array) {
    std::vector<ParcelInfo> result;
    result.reserve(array.size());
    for (const auto& item : array) {
        if (item.is_object()) {
            result.push_back(
                parse_parcel(item));
        }
    }
    return result;
}

std::vector<ParcelAccessEntry> parse_access(
    const Json& array) {
    std::vector<ParcelAccessEntry> result;
    result.reserve(array.size());
    for (const auto& item : array) {
        if (!item.is_object()) continue;
        result.push_back({
            .user_id =
                string_value(item, "user_id"),
            .allowed =
                bool_value(item, "allowed"),
            .updated_unix =
                i64_value(item, "updated_unix"),
        });
    }
    return result;
}

TravelSession parse_travel_session(
    const Json& doc) {
    if (!doc.is_object()) {
        throw std::runtime_error(
            "Travel response must be an object");
    }

    TravelSession result;
    if (const auto user = doc.find("user");
        user != doc.end() && user->is_object()) {
        result.user_id =
            string_value(*user, "id");
        result.display_name =
            string_value(*user, "display_name");
    }
    if (const auto region = doc.find("region");
        region != doc.end() && region->is_object()) {
        result.region_id =
            string_value(*region, "id");
        result.region_name =
            string_value(*region, "name");
    }

    result.scene_endpoint =
        string_value(doc, "scene_endpoint");
    result.scene_ticket =
        string_value(doc, "scene_ticket");
    result.handoff_from =
        string_value(doc, "handoff_from");
    result.crossing_id =
        string_value(doc, "crossing_id");
    result.expires_unix =
        i64_value(doc, "expires_unix");

    if (const auto capabilities =
            doc.find("capabilities");
        capabilities != doc.end()) {
        if (capabilities->is_string()) {
            result.capabilities =
                csv_list(
                    capabilities
                        ->get_ref<const std::string&>());
        } else if (capabilities->is_array()) {
            for (const auto& value : *capabilities) {
                if (value.is_string()) {
                    result.capabilities.push_back(
                        value.get<std::string>());
                }
            }
        }
    }

    if (const auto groups = doc.find("groups");
        groups != doc.end()) {
        if (groups->is_string()) {
            result.groups =
                csv_list(
                    groups
                        ->get_ref<const std::string&>());
        } else if (groups->is_array()) {
            for (const auto& value : *groups) {
                if (value.is_string()) {
                    result.groups.push_back(
                        value.get<std::string>());
                }
            }
        }
    }

    if (const auto spawn = doc.find("spawn");
        spawn != doc.end() &&
        spawn->is_object()) {
        result.spawn = {
            .x = double_value(*spawn, "x"),
            .y = double_value(*spawn, "y"),
            .z = double_value(*spawn, "z"),
        };
    }

    if (result.region_id.empty() ||
        result.scene_endpoint.empty() ||
        result.scene_ticket.empty()) {
        throw std::runtime_error(
            "Travel response is missing Scene session fields");
    }
    return result;
}

CrossingInfo parse_crossing(
    const Json& doc) {
    if (!doc.is_object()) {
        throw std::runtime_error(
            "Handoff crossing response must be an object");
    }

    CrossingInfo result{
        .id = string_value(doc, "id"),
        .user_id =
            string_value(doc, "user_id"),
        .from_region =
            string_value(doc, "from_region"),
        .to_region =
            string_value(doc, "to_region"),
        .state = string_value(doc, "state"),
        .reservation_token =
            string_value(doc, "reservation_token"),
        .rollback_reason =
            string_value(doc, "rollback_reason"),
        .created_unix =
            i64_value(doc, "created_unix"),
        .expires_unix =
            i64_value(doc, "expires_unix"),
        .reserved_unix =
            i64_value(doc, "reserved_unix"),
        .completed_unix =
            i64_value(doc, "completed_unix"),
        .rolled_back_unix =
            i64_value(doc, "rolled_back_unix"),
    };

    if (result.id.empty()) {
        throw std::runtime_error(
            "Handoff crossing response is missing id");
    }
    return result;
}

} // namespace

PlatformClient::PlatformClient(
    HttpTransport& transport)
    : transport_(transport) {}

AvatarAppearanceSnapshot
PlatformClient::appearance(
    const std::string& base,
    const std::string& token) const {
    const auto doc = request(
        transport_,
        base,
        token,
        "GET",
        "/v1/avatar/appearance");
    return parse_appearance_snapshot(
        doc.dump());
}

AvatarAppearanceSnapshot
PlatformClient::set_wearable(
    const std::string& base,
    const std::string& token,
    std::string_view slot,
    std::string_view item_id,
    std::string_view asset_id) const {
    const auto doc = request(
        transport_,
        base,
        token,
        "POST",
        "/v1/avatar/appearance/wearable",
        {
            {"slot",
             required_text(slot, "wearable slot", 128U)},
            {"item_id",
             required_text(item_id, "Inventory item id", 256U)},
            {"asset_id",
             required_text(asset_id, "Asset id", 256U)},
        });
    return parse_appearance_snapshot(
        doc.dump());
}

AvatarAppearanceSnapshot
PlatformClient::remove_wearable(
    const std::string& base,
    const std::string& token,
    std::string_view slot) const {
    const auto doc = request(
        transport_,
        base,
        token,
        "POST",
        "/v1/avatar/appearance/wearable/remove",
        {
            {"slot",
             required_text(slot, "wearable slot", 128U)},
        });
    return parse_appearance_snapshot(
        doc.dump());
}

AvatarAppearanceSnapshot PlatformClient::attach(
    const std::string& base,
    const std::string& token,
    std::string_view point,
    std::string_view item_id,
    std::string_view asset_id) const {
    const auto doc = request(
        transport_,
        base,
        token,
        "POST",
        "/v1/avatar/appearance/attachment",
        {
            {"point",
             required_text(point, "attachment point", 128U)},
            {"item_id",
             required_text(item_id, "Inventory item id", 256U)},
            {"asset_id",
             required_text(asset_id, "Asset id", 256U)},
        });
    return parse_appearance_snapshot(
        doc.dump());
}

AvatarAppearanceSnapshot PlatformClient::detach(
    const std::string& base,
    const std::string& token,
    std::string_view point,
    std::string_view item_id) const {
    Json body{
        {"point",
         required_text(point, "attachment point", 128U)},
    };
    if (!item_id.empty()) {
        body["item_id"] =
            required_text(
                item_id,
                "Inventory item id",
                256U);
    }

    const auto doc = request(
        transport_,
        base,
        token,
        "POST",
        "/v1/avatar/appearance/attachment/remove",
        body);
    return parse_appearance_snapshot(
        doc.dump());
}

InventorySnapshot PlatformClient::inventory(
    const std::string& base,
    const std::string& token) const {
    const auto doc = request(
        transport_,
        base,
        token,
        "GET",
        "/v1/inventory");
    return parse_inventory_snapshot(
        doc.dump());
}

InventoryFolderSnapshot
PlatformClient::create_folder(
    const std::string& base,
    const std::string& token,
    std::string_view parent_id,
    std::string_view name) const {
    Json body{
        {"name",
         required_text(
             name,
             "folder name",
             256U)},
    };
    if (!parent_id.empty()) {
        body["parent_id"] =
            required_text(
                parent_id,
                "parent folder id",
                256U);
    }

    const auto doc = request(
        transport_,
        base,
        token,
        "POST",
        "/v1/inventory/folders",
        body);
    const auto found = doc.find("folder");
    if (found == doc.end() ||
        !found->is_object()) {
        throw std::runtime_error(
            "Folder creation response is missing folder");
    }

    return {
        .id = string_value(*found, "id"),
        .parent_id =
            string_value(*found, "parent_id"),
        .name = string_value(*found, "name"),
    };
}

InventoryItemSnapshot PlatformClient::create_item(
    const std::string& base,
    const std::string& token,
    std::string_view parent_id,
    std::string_view asset_id,
    std::string_view name) const {
    Json body{
        {"asset_id",
         required_text(
             asset_id,
             "Asset id",
             256U)},
        {"name",
         required_text(
             name,
             "Inventory item name",
             256U)},
    };
    if (!parent_id.empty()) {
        body["parent_id"] =
            required_text(
                parent_id,
                "parent folder id",
                256U);
    }

    const auto doc = request(
        transport_,
        base,
        token,
        "POST",
        "/v1/inventory/items",
        body);
    const auto found = doc.find("item");
    if (found == doc.end() ||
        !found->is_object()) {
        throw std::runtime_error(
            "Item creation response is missing item");
    }

    return {
        .id = string_value(*found, "id"),
        .parent_id = std::string(parent_id),
        .asset_id =
            string_value(*found, "asset_id"),
        .name = string_value(*found, "name"),
    };
}

std::vector<PresenceInfo>
PlatformClient::presences(
    const std::string& base,
    const std::string& token) const {
    const auto doc = request(
        transport_,
        base,
        token,
        "GET",
        "/v1/presence");
    return parse_presences(
        array_field(doc, "presences"));
}

std::vector<FriendRelation>
PlatformClient::friends(
    const std::string& base,
    const std::string& token) const {
    const auto doc = request(
        transport_,
        base,
        token,
        "GET",
        "/v1/social/friends");
    return parse_friends(
        array_field(doc, "friends"));
}

void PlatformClient::request_friend(
    const std::string& base,
    const std::string& token,
    std::string_view user_id) const {
    (void)request(
        transport_, base, token,
        "POST",
        "/v1/social/friends/request",
        {{"user_id",
          required_text(user_id, "user id", 256U)}});
}

void PlatformClient::accept_friend(
    const std::string& base,
    const std::string& token,
    std::string_view user_id) const {
    (void)request(
        transport_, base, token,
        "POST",
        "/v1/social/friends/accept",
        {{"user_id",
          required_text(user_id, "user id", 256U)}});
}

void PlatformClient::remove_friend(
    const std::string& base,
    const std::string& token,
    std::string_view user_id) const {
    (void)request(
        transport_, base, token,
        "POST",
        "/v1/social/friends/remove",
        {{"user_id",
          required_text(user_id, "user id", 256U)}});
}

MessageList PlatformClient::messages(
    const std::string& base,
    const std::string& token) const {
    const auto doc = request(
        transport_,
        base,
        token,
        "GET",
        "/v1/social/messages");

    return {
        .unread =
            static_cast<std::size_t>(
                u64_value(doc, "unread")),
        .messages =
            parse_messages(
                array_field(doc, "messages")),
    };
}

void PlatformClient::send_message(
    const std::string& base,
    const std::string& token,
    std::string_view recipient_id,
    std::string_view text) const {
    (void)request(
        transport_, base, token,
        "POST",
        "/v1/social/messages",
        {
            {"recipient_id",
             required_text(
                 recipient_id,
                 "recipient id",
                 256U)},
            {"text",
             required_text(
                 text,
                 "message text",
                 2000U)},
        });
}

void PlatformClient::mark_message_read(
    const std::string& base,
    const std::string& token,
    std::string_view message_id) const {
    (void)request(
        transport_, base, token,
        "POST",
        "/v1/social/messages/read",
        {{"message_id",
          required_text(
              message_id,
              "message id",
              256U)}});
}

std::vector<SocialPolicy>
PlatformClient::social_policies(
    const std::string& base,
    const std::string& token) const {
    const auto doc = request(
        transport_,
        base,
        token,
        "GET",
        "/v1/social/policies");
    return parse_policies(
        array_field(doc, "policies"));
}

void PlatformClient::set_blocked(
    const std::string& base,
    const std::string& token,
    std::string_view user_id,
    bool blocked) const {
    (void)request(
        transport_, base, token,
        "POST",
        "/v1/social/block",
        {
            {"user_id",
             required_text(
                 user_id,
                 "user id",
                 256U)},
            {"blocked", blocked},
        });
}

void PlatformClient::set_muted(
    const std::string& base,
    const std::string& token,
    std::string_view user_id,
    bool muted) const {
    (void)request(
        transport_, base, token,
        "POST",
        "/v1/social/mute",
        {
            {"user_id",
             required_text(
                 user_id,
                 "user id",
                 256U)},
            {"muted", muted},
        });
}

NotificationList PlatformClient::notifications(
    const std::string& base,
    const std::string& token) const {
    const auto doc = request(
        transport_,
        base,
        token,
        "GET",
        "/v1/notifications");
    return {
        .unread =
            static_cast<std::size_t>(
                u64_value(doc, "unread")),
        .notifications =
            parse_notifications(
                array_field(
                    doc,
                    "notifications")),
    };
}

void PlatformClient::mark_notification_read(
    const std::string& base,
    const std::string& token,
    std::string_view notification_id) const {
    (void)request(
        transport_, base, token,
        "POST",
        "/v1/notifications/read",
        {{"notification_id",
          required_text(
              notification_id,
              "notification id",
              256U)}});
}

std::vector<GroupInfo> PlatformClient::groups(
    const std::string& base,
    const std::string& token) const {
    const auto doc = request(
        transport_,
        base,
        token,
        "GET",
        "/v1/groups");
    return parse_groups(
        array_field(doc, "groups"));
}

std::string PlatformClient::create_group(
    const std::string& base,
    const std::string& token,
    std::string_view name) const {
    const auto doc = request(
        transport_, base, token,
        "POST",
        "/v1/groups",
        {{"name",
          required_text(
              name,
              "group name",
              256U)}});
    const auto id =
        string_value(doc, "group_id");
    if (id.empty()) {
        throw std::runtime_error(
            "Group creation response is missing group_id");
    }
    return id;
}

std::vector<GroupMember>
PlatformClient::group_members(
    const std::string& base,
    const std::string& token,
    std::string_view group_id) const {
    const auto id =
        path_component(group_id, "group id");
    const auto doc = request(
        transport_,
        base,
        token,
        "GET",
        "/v1/groups/" + id + "/members");
    return parse_members(
        array_field(doc, "members"));
}

void PlatformClient::add_group_member(
    const std::string& base,
    const std::string& token,
    std::string_view group_id,
    std::string_view user_id,
    std::string_view role) const {
    (void)request(
        transport_, base, token,
        "POST",
        "/v1/groups/members",
        {
            {"group_id",
             required_text(
                 group_id,
                 "group id",
                 256U)},
            {"user_id",
             required_text(
                 user_id,
                 "user id",
                 256U)},
            {"role",
             required_text(
                 role,
                 "group role",
                 64U)},
        });
}

void PlatformClient::set_group_role(
    const std::string& base,
    const std::string& token,
    std::string_view group_id,
    std::string_view user_id,
    std::string_view role) const {
    (void)request(
        transport_, base, token,
        "POST",
        "/v1/groups/role",
        {
            {"group_id",
             required_text(
                 group_id,
                 "group id",
                 256U)},
            {"user_id",
             required_text(
                 user_id,
                 "user id",
                 256U)},
            {"role",
             required_text(
                 role,
                 "group role",
                 64U)},
        });
}

void PlatformClient::remove_group_member(
    const std::string& base,
    const std::string& token,
    std::string_view group_id,
    std::string_view user_id) const {
    (void)request(
        transport_, base, token,
        "POST",
        "/v1/groups/remove",
        {
            {"group_id",
             required_text(
                 group_id,
                 "group id",
                 256U)},
            {"user_id",
             required_text(
                 user_id,
                 "user id",
                 256U)},
        });
}

std::vector<GroupInvite>
PlatformClient::group_invites(
    const std::string& base,
    const std::string& token) const {
    const auto doc = request(
        transport_,
        base,
        token,
        "GET",
        "/v1/groups/invites");
    return parse_invites(
        array_field(doc, "invites"));
}

void PlatformClient::invite_group_member(
    const std::string& base,
    const std::string& token,
    std::string_view group_id,
    std::string_view user_id,
    std::string_view role,
    std::int64_t lifetime_seconds) const {
    const auto lifetime =
        std::clamp<std::int64_t>(
            lifetime_seconds,
            60,
            2592000);
    (void)request(
        transport_, base, token,
        "POST",
        "/v1/groups/invites",
        {
            {"group_id",
             required_text(
                 group_id,
                 "group id",
                 256U)},
            {"user_id",
             required_text(
                 user_id,
                 "user id",
                 256U)},
            {"role",
             required_text(
                 role,
                 "group role",
                 64U)},
            {"lifetime_seconds", lifetime},
        });
}

void PlatformClient::accept_group_invite(
    const std::string& base,
    const std::string& token,
    std::string_view invite_id) const {
    (void)request(
        transport_, base, token,
        "POST",
        "/v1/groups/invites/accept",
        {{"invite_id",
          required_text(
              invite_id,
              "invite id",
              256U)}});
}

void PlatformClient::revoke_group_invite(
    const std::string& base,
    const std::string& token,
    std::string_view invite_id) const {
    (void)request(
        transport_, base, token,
        "POST",
        "/v1/groups/invites/revoke",
        {{"invite_id",
          required_text(
              invite_id,
              "invite id",
              256U)}});
}

std::vector<GroupPost> PlatformClient::group_channel(
    const std::string& base,
    const std::string& token,
    std::string_view group_id) const {
    const auto id =
        path_component(group_id, "group id");
    const auto doc = request(
        transport_,
        base,
        token,
        "GET",
        "/v1/groups/" + id + "/channel");
    return parse_posts(
        array_field(doc, "posts"));
}

void PlatformClient::send_group_post(
    const std::string& base,
    const std::string& token,
    std::string_view group_id,
    std::string_view kind,
    std::string_view title,
    std::string_view text) const {
    const auto kind_value =
        required_text(
            kind,
            "group post kind",
            32U);
    if (kind_value != "chat" &&
        kind_value != "notice") {
        throw std::invalid_argument(
            "Group post kind must be chat or notice");
    }

    Json body{
        {"group_id",
         required_text(
             group_id,
             "group id",
             256U)},
        {"kind", kind_value},
        {"text",
         required_text(
             text,
             "group post text",
             2000U)},
    };
    if (!title.empty()) {
        body["title"] =
            required_text(
                title,
                "group post title",
                256U);
    }
    (void)request(
        transport_, base, token,
        "POST",
        "/v1/groups/channel",
        body);
}

std::vector<ParcelInfo>
PlatformClient::region_parcels(
    const std::string& base,
    const std::string& token,
    std::string_view region_id) const {
    const auto id =
        path_component(region_id, "Region id");
    const auto doc = request(
        transport_,
        base,
        token,
        "GET",
        "/v1/regions/" + id + "/parcels");
    return parse_parcels(
        array_field(doc, "parcels"));
}

std::vector<ParcelInfo>
PlatformClient::owned_parcels(
    const std::string& base,
    const std::string& token) const {
    const auto doc = request(
        transport_,
        base,
        token,
        "GET",
        "/v1/parcels/owned");
    return parse_parcels(
        array_field(doc, "parcels"));
}

std::string PlatformClient::create_parcel(
    const std::string& base,
    const std::string& token,
    const ParcelInfo& parcel) const {
    if (parcel.x1 > parcel.x2 ||
        parcel.y1 > parcel.y2) {
        throw std::invalid_argument(
            "Parcel bounds are inverted");
    }

    const auto doc = request(
        transport_, base, token,
        "POST",
        "/v1/parcels",
        {
            {"region_id",
             required_text(
                 parcel.region_id,
                 "Region id",
                 256U)},
            {"name",
             required_text(
                 parcel.name,
                 "parcel name",
                 256U)},
            {"x1", parcel.x1},
            {"y1", parcel.y1},
            {"x2", parcel.x2},
            {"y2", parcel.y2},
        });

    const auto id =
        string_value(doc, "parcel_id");
    if (id.empty()) {
        throw std::runtime_error(
            "Parcel creation response is missing parcel_id");
    }
    return id;
}

void PlatformClient::update_parcel_policy(
    const std::string& base,
    const std::string& token,
    const ParcelInfo& parcel) const {
    Json body{
        {"parcel_id",
         required_text(
             parcel.id,
             "parcel id",
             256U)},
        {"public_entry", parcel.public_entry},
        {"public_build", parcel.public_build},
        {"group_build", parcel.group_build},
        {"group_terraform",
         parcel.group_terraform},
    };
    if (!parcel.group_id.empty()) {
        body["group_id"] = parcel.group_id;
    }

    (void)request(
        transport_, base, token,
        "POST",
        "/v1/parcels/policy",
        body);
}

std::vector<ParcelAccessEntry>
PlatformClient::parcel_access(
    const std::string& base,
    const std::string& token,
    std::string_view parcel_id) const {
    const auto id =
        path_component(parcel_id, "parcel id");
    const auto doc = request(
        transport_,
        base,
        token,
        "GET",
        "/v1/parcels/" + id + "/access");
    return parse_access(
        array_field(doc, "access"));
}

void PlatformClient::set_parcel_access(
    const std::string& base,
    const std::string& token,
    std::string_view parcel_id,
    std::string_view user_id,
    bool allowed) const {
    (void)request(
        transport_, base, token,
        "POST",
        "/v1/parcels/access",
        {
            {"parcel_id",
             required_text(
                 parcel_id,
                 "parcel id",
                 256U)},
            {"user_id",
             required_text(
                 user_id,
                 "user id",
                 256U)},
            {"allowed", allowed},
        });
}

void PlatformClient::remove_parcel_access(
    const std::string& base,
    const std::string& token,
    std::string_view parcel_id,
    std::string_view user_id) const {
    (void)request(
        transport_, base, token,
        "POST",
        "/v1/parcels/access/remove",
        {
            {"parcel_id",
             required_text(
                 parcel_id,
                 "parcel id",
                 256U)},
            {"user_id",
             required_text(
                 user_id,
                 "user id",
                 256U)},
        });
}

TravelSession PlatformClient::teleport(
    const std::string& base,
    const std::string& token,
    std::string_view region_id,
    double x,
    double y,
    double z) const {
    if (!std::isfinite(x) ||
        !std::isfinite(y) ||
        !std::isfinite(z)) {
        throw std::invalid_argument(
            "Teleport coordinates must be finite");
    }

    return parse_travel_session(
        request(
            transport_, base, token,
            "POST",
            "/v1/viewer/teleport",
            {
                {"region",
                 required_text(
                     region_id,
                     "Region id",
                     256U)},
                {"x", x},
                {"y", y},
                {"z", z},
            }));
}

TravelSession PlatformClient::handoff(
    const std::string& base,
    const std::string& token,
    std::string_view from_region,
    std::string_view to_region,
    double vx,
    double vy,
    double vz,
    double rx,
    double ry,
    double rz) const {
    const double values[]{
        vx, vy, vz, rx, ry, rz};
    if (!std::all_of(
            std::begin(values),
            std::end(values),
            [](double value) {
                return std::isfinite(value);
            })) {
        throw std::invalid_argument(
            "Handoff motion values must be finite");
    }

    return parse_travel_session(
        request(
            transport_, base, token,
            "POST",
            "/v1/viewer/handoff",
            {
                {"from_region",
                 required_text(
                     from_region,
                     "source Region",
                     256U)},
                {"to_region",
                 required_text(
                     to_region,
                     "destination Region",
                     256U)},
                {"vx", vx},
                {"vy", vy},
                {"vz", vz},
                {"rx", rx},
                {"ry", ry},
                {"rz", rz},
                {"avx", 0.0},
                {"avy", 0.0},
                {"avz", 0.0},
            }));
}

CrossingInfo PlatformClient::reserve_handoff(
    const std::string& base,
    const std::string& token,
    std::string_view crossing_id,
    std::string_view region_id) const {
    return parse_crossing(
        request(
            transport_, base, token,
            "POST",
            "/v1/viewer/handoff/reserve",
            {
                {"crossing_id",
                 required_text(
                     crossing_id,
                     "crossing id",
                     256U)},
                {"region",
                 required_text(
                     region_id,
                     "Region id",
                     256U)},
            }));
}

CrossingInfo PlatformClient::complete_handoff(
    const std::string& base,
    const std::string& token,
    std::string_view crossing_id,
    std::string_view region_id,
    std::string_view reservation_token) const {
    return parse_crossing(
        request(
            transport_, base, token,
            "POST",
            "/v1/viewer/handoff/complete",
            {
                {"crossing_id",
                 required_text(
                     crossing_id,
                     "crossing id",
                     256U)},
                {"region",
                 required_text(
                     region_id,
                     "Region id",
                     256U)},
                {"reservation_token",
                 required_text(
                     reservation_token,
                     "reservation token",
                     2048U)},
            }));
}

CrossingInfo PlatformClient::rollback_handoff(
    const std::string& base,
    const std::string& token,
    std::string_view crossing_id,
    std::string_view reason) const {
    return parse_crossing(
        request(
            transport_, base, token,
            "POST",
            "/v1/viewer/handoff/rollback",
            {
                {"crossing_id",
                 required_text(
                     crossing_id,
                     "crossing id",
                     256U)},
                {"reason",
                 required_text(
                     reason,
                     "rollback reason",
                     512U)},
            }));
}

} // namespace ogl::viewer::core
