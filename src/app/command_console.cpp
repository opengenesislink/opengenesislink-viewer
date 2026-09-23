#include "opengenesislink/viewer/app/command_console.hpp"

#include <cctype>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ogl::viewer::app {
namespace {

std::vector<std::string> tokenize(
    std::string_view input) {
    std::vector<std::string> result;
    std::string current;
    bool quoted = false;
    bool escaped = false;

    for (const char character : input) {
        if (escaped) {
            current.push_back(character);
            escaped = false;
            continue;
        }
        if (character == '\\') {
            escaped = true;
            continue;
        }
        if (character == '"') {
            quoted = !quoted;
            continue;
        }
        if (!quoted &&
            std::isspace(
                static_cast<unsigned char>(
                    character)) != 0) {
            if (!current.empty()) {
                result.push_back(
                    std::move(current));
                current.clear();
            }
            continue;
        }
        current.push_back(character);
    }

    if (escaped || quoted) {
        throw std::invalid_argument(
            "Console command has an unfinished escape or quote");
    }
    if (!current.empty()) {
        result.push_back(std::move(current));
    }
    return result;
}

void require_args(
    const std::vector<std::string>& tokens,
    std::size_t minimum,
    std::string_view usage) {
    if (tokens.size() < minimum + 1U) {
        throw std::invalid_argument(
            "Usage: " + std::string(usage));
    }
}

ConsoleCommand command(
    ConsoleCommandKind kind,
    const std::vector<std::string>& tokens,
    std::size_t first_argument = 1U) {
    ConsoleCommand result;
    result.kind = kind;
    if (first_argument < tokens.size()) {
        result.arguments.assign(
            tokens.begin() +
                static_cast<std::ptrdiff_t>(
                    first_argument),
            tokens.end());
    }
    return result;
}

} // namespace

ConsoleCommand parse_console_command(
    std::string_view input) {
    const auto tokens = tokenize(input);
    if (tokens.empty()) {
        throw std::invalid_argument(
            "Console command must not be empty");
    }

    if (!tokens[0].starts_with('/')) {
        ConsoleCommand result;
        result.kind =
            ConsoleCommandKind::local_chat;
        result.arguments.emplace_back(input);
        return result;
    }

    const auto& name = tokens[0];

    if (name == "/help") {
        return command(
            ConsoleCommandKind::help,
            tokens);
    }
    if (name == "/say") {
        require_args(tokens, 1U, "/say <text>");
        return command(
            ConsoleCommandKind::local_chat,
            tokens);
    }
    if (name == "/dm") {
        require_args(
            tokens, 2U,
            "/dm <user-id> <text>");
        return command(
            ConsoleCommandKind::direct_message,
            tokens);
    }
    if (name == "/social" &&
        tokens.size() == 2U &&
        tokens[1] == "refresh") {
        return command(
            ConsoleCommandKind::social_refresh,
            tokens,
            2U);
    }
    if (name == "/friend") {
        require_args(
            tokens, 2U,
            "/friend request|accept|remove <user-id>");
        if (tokens[1] == "request") {
            return command(
                ConsoleCommandKind::friend_request,
                tokens,
                2U);
        }
        if (tokens[1] == "accept") {
            return command(
                ConsoleCommandKind::friend_accept,
                tokens,
                2U);
        }
        if (tokens[1] == "remove") {
            return command(
                ConsoleCommandKind::friend_remove,
                tokens,
                2U);
        }
    }
    if (name == "/block" ||
        name == "/mute") {
        require_args(
            tokens, 2U,
            name == "/block"
                ? "/block <user-id> on|off"
                : "/mute <user-id> on|off");
        return command(
            name == "/block"
                ? ConsoleCommandKind::block
                : ConsoleCommandKind::mute,
            tokens);
    }
    if (name == "/appearance") {
        if (tokens.size() == 2U &&
            tokens[1] == "refresh") {
            return command(
                ConsoleCommandKind::appearance_refresh,
                tokens,
                2U);
        }
    }
    if (name == "/wear") {
        require_args(
            tokens, 3U,
            "/wear <slot> <item-id> <asset-id>");
        return command(
            ConsoleCommandKind::wearable_set,
            tokens);
    }
    if (name == "/unwear") {
        require_args(
            tokens, 1U,
            "/unwear <slot>");
        return command(
            ConsoleCommandKind::wearable_remove,
            tokens);
    }
    if (name == "/attach") {
        require_args(
            tokens, 3U,
            "/attach <point> <item-id> <asset-id>");
        return command(
            ConsoleCommandKind::attachment_set,
            tokens);
    }
    if (name == "/detach") {
        require_args(
            tokens, 1U,
            "/detach <point> [item-id]");
        return command(
            ConsoleCommandKind::attachment_remove,
            tokens);
    }
    if (name == "/inventory") {
        require_args(
            tokens, 1U,
            "/inventory refresh|folder|item ...");
        if (tokens[1] == "refresh") {
            return command(
                ConsoleCommandKind::inventory_refresh,
                tokens,
                2U);
        }
        if (tokens[1] == "folder") {
            require_args(
                tokens, 3U,
                "/inventory folder <parent-id|-> <name>");
            return command(
                ConsoleCommandKind::inventory_folder_create,
                tokens,
                2U);
        }
        if (tokens[1] == "item") {
            require_args(
                tokens, 4U,
                "/inventory item <parent-id|-> <asset-id> <name>");
            return command(
                ConsoleCommandKind::inventory_item_create,
                tokens,
                2U);
        }
    }
    if (name == "/groups") {
        if (tokens.size() == 2U &&
            tokens[1] == "refresh") {
            return command(
                ConsoleCommandKind::groups_refresh,
                tokens,
                2U);
        }
    }
    if (name == "/group") {
        require_args(
            tokens, 1U,
            "/group create|invite|accept|add|role|remove|post ...");
        if (tokens[1] == "create") {
            require_args(
                tokens, 2U,
                "/group create <name>");
            return command(
                ConsoleCommandKind::group_create,
                tokens,
                2U);
        }
        if (tokens[1] == "info") {
            require_args(
                tokens, 2U,
                "/group info <group-id>");
            return command(
                ConsoleCommandKind::group_details,
                tokens,
                2U);
        }
        if (tokens[1] == "invite") {
            require_args(
                tokens, 3U,
                "/group invite <group-id> <user-id> [role]");
            return command(
                ConsoleCommandKind::group_invite,
                tokens,
                2U);
        }
        if (tokens[1] == "accept") {
            require_args(
                tokens, 2U,
                "/group accept <invite-id>");
            return command(
                ConsoleCommandKind::group_invite_accept,
                tokens,
                2U);
        }
        if (tokens[1] == "add") {
            require_args(
                tokens, 3U,
                "/group add <group-id> <user-id> [role]");
            return command(
                ConsoleCommandKind::group_member_add,
                tokens,
                2U);
        }
        if (tokens[1] == "role") {
            require_args(
                tokens, 4U,
                "/group role <group-id> <user-id> <role>");
            return command(
                ConsoleCommandKind::group_role,
                tokens,
                2U);
        }
        if (tokens[1] == "remove") {
            require_args(
                tokens, 3U,
                "/group remove <group-id> <user-id>");
            return command(
                ConsoleCommandKind::group_member_remove,
                tokens,
                2U);
        }
        if (tokens[1] == "post") {
            require_args(
                tokens, 4U,
                "/group post <group-id> chat|notice <text>");
            return command(
                ConsoleCommandKind::group_post,
                tokens,
                2U);
        }
    }
    if (name == "/notifications") {
        if (tokens.size() == 2U &&
            tokens[1] == "refresh") {
            return command(
                ConsoleCommandKind::notifications_refresh,
                tokens,
                2U);
        }
    }
    if (name == "/notify") {
        require_args(
            tokens, 2U,
            "/notify read <notification-id>");
        if (tokens[1] == "read") {
            return command(
                ConsoleCommandKind::notification_read,
                tokens,
                2U);
        }
    }
    if (name == "/parcels") {
        if (tokens.size() == 2U &&
            tokens[1] == "refresh") {
            return command(
                ConsoleCommandKind::parcels_refresh,
                tokens,
                2U);
        }
    }
    if (name == "/parcel") {
        require_args(
            tokens, 1U,
            "/parcel create|policy|access|access-remove ...");
        if (tokens[1] == "create") {
            require_args(
                tokens, 6U,
                "/parcel create <name> <x1> <y1> <x2> <y2>");
            return command(
                ConsoleCommandKind::parcel_create,
                tokens,
                2U);
        }
        if (tokens[1] == "policy") {
            require_args(
                tokens, 6U,
                "/parcel policy <id> <group-id|-> <entry 0|1> <public-build 0|1> <group-build 0|1> [group-terraform 0|1]");
            return command(
                ConsoleCommandKind::parcel_policy,
                tokens,
                2U);
        }
        if (tokens[1] == "access-list") {
            require_args(
                tokens, 2U,
                "/parcel access-list <parcel-id>");
            return command(
                ConsoleCommandKind::parcel_access_list,
                tokens,
                2U);
        }
        if (tokens[1] == "access") {
            require_args(
                tokens, 4U,
                "/parcel access <parcel-id> <user-id> allow|deny");
            return command(
                ConsoleCommandKind::parcel_access,
                tokens,
                2U);
        }
        if (tokens[1] == "access-remove") {
            require_args(
                tokens, 3U,
                "/parcel access-remove <parcel-id> <user-id>");
            return command(
                ConsoleCommandKind::parcel_access_remove,
                tokens,
                2U);
        }
    }
    if (name == "/tp") {
        require_args(
            tokens, 1U,
            "/tp <region-id> [x y z]");
        return command(
            ConsoleCommandKind::teleport,
            tokens);
    }
    if (name == "/handoff") {
        require_args(
            tokens, 1U,
            "/handoff <adjacent-region-id>");
        return command(
            ConsoleCommandKind::handoff,
            tokens);
    }
    if (name == "/build") {
        require_args(
            tokens, 1U,
            "/build list|create|delete|move|scale|text|link|unlink|permissions|motion|touch|shape|material|force ...");
        if (tokens[1] == "list") {
            return command(
                ConsoleCommandKind::build_list,
                tokens,
                2U);
        }
        if (tokens[1] == "create") {
            require_args(
                tokens, 5U,
                "/build create <name> <x> <y> <z> [physical on|off]");
            return command(
                ConsoleCommandKind::build_create,
                tokens,
                2U);
        }
        if (tokens[1] == "delete") {
            require_args(
                tokens, 2U,
                "/build delete <entity-id>");
            return command(
                ConsoleCommandKind::build_delete,
                tokens,
                2U);
        }
        if (tokens[1] == "move") {
            require_args(
                tokens, 5U,
                "/build move <entity-id> <x> <y> <z>");
            return command(
                ConsoleCommandKind::build_move,
                tokens,
                2U);
        }
        if (tokens[1] == "scale") {
            require_args(
                tokens, 5U,
                "/build scale <entity-id> <sx> <sy> <sz>");
            return command(
                ConsoleCommandKind::build_scale,
                tokens,
                2U);
        }
        if (tokens[1] == "text") {
            require_args(
                tokens, 3U,
                "/build text <entity-id> <text>");
            return command(
                ConsoleCommandKind::build_text,
                tokens,
                2U);
        }
        if (tokens[1] == "link" ||
            tokens[1] == "unlink") {
            require_args(
                tokens, 3U,
                "/build link|unlink <root-id> <child-id>");
            return command(
                tokens[1] == "link"
                    ? ConsoleCommandKind::build_link
                    : ConsoleCommandKind::build_unlink,
                tokens,
                2U);
        }
        if (tokens[1] == "permissions") {
            require_args(
                tokens, 5U,
                "/build permissions <entity-id> <group-id|-> <group-mask> <everyone-mask>");
            return command(
                ConsoleCommandKind::build_permissions,
                tokens,
                2U);
        }
        if (tokens[1] == "motion") {
            require_args(
                tokens, 8U,
                "/build motion <entity-id> <vx> <vy> <vz> <avx> <avy> <avz>");
            return command(
                ConsoleCommandKind::build_motion,
                tokens,
                2U);
        }
        if (tokens[1] == "touch") {
            require_args(
                tokens, 3U,
                "/build touch <entity-id> start|touch|end");
            return command(
                ConsoleCommandKind::build_interact,
                tokens,
                2U);
        }
        if (tokens[1] == "shape") {
            require_args(
                tokens, 3U,
                "/build shape <entity-id> sphere|box|capsule");
            return command(
                ConsoleCommandKind::build_shape,
                tokens,
                2U);
        }
        if (tokens[1] == "material") {
            require_args(
                tokens, 5U,
                "/build material <entity-id> <mass> <restitution> <friction>");
            return command(
                ConsoleCommandKind::build_material,
                tokens,
                2U);
        }
        if (tokens[1] == "force") {
            require_args(
                tokens, 5U,
                "/build force <entity-id> <x> <y> <z>");
            return command(
                ConsoleCommandKind::build_force,
                tokens,
                2U);
        }
    }
    if (name == "/terrain") {
        require_args(
            tokens, 4U,
            "/terrain set <grid-x> <grid-y> <height>");
        if (tokens[1] == "set") {
            return command(
                ConsoleCommandKind::terrain_set,
                tokens,
                2U);
        }
    }
    if (name == "/refresh") {
        return command(
            ConsoleCommandKind::refresh_all,
            tokens);
    }

    throw std::invalid_argument(
        "Unknown Viewer command. Use /help");
}

std::string console_help() {
    return
        "TEXT = LOCAL CHAT | /dm USER TEXT | /social refresh | /friend request|accept|remove USER | "
        "/block USER on|off | /mute USER on|off | /appearance refresh | "
        "/wear SLOT ITEM ASSET | /unwear SLOT | /attach POINT ITEM ASSET | "
        "/detach POINT [ITEM] | /inventory refresh|folder|item ... | "
        "/groups refresh | /group create|info|invite|accept|add|role|remove|post ... | "
        "/notifications refresh | /notify read ID | /parcels refresh | "
        "/parcel create|policy|access-list|access|access-remove ... | /tp REGION [X Y Z] | "
        "/handoff REGION | /build list|create|delete|move|scale|text|link|unlink|permissions|motion|touch|shape|material|force ... | "
        "/terrain set X Y HEIGHT | /refresh";
}

} // namespace ogl::viewer::app
