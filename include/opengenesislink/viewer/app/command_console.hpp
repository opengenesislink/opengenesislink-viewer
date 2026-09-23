#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ogl::viewer::app {

enum class ConsoleCommandKind {
    help,
    local_chat,
    direct_message,
    friend_request,
    friend_accept,
    friend_remove,
    block,
    mute,
    appearance_refresh,
    wearable_set,
    wearable_remove,
    attachment_set,
    attachment_remove,
    inventory_refresh,
    inventory_folder_create,
    inventory_item_create,
    groups_refresh,
    group_create,
    group_invite,
    group_invite_accept,
    group_member_add,
    group_role,
    group_member_remove,
    group_post,
    notifications_refresh,
    notification_read,
    parcels_refresh,
    parcel_create,
    parcel_policy,
    parcel_access,
    parcel_access_remove,
    teleport,
    handoff,
    build_create,
    build_delete,
    build_move,
    build_scale,
    build_text,
    build_link,
    build_unlink,
    build_shape,
    build_material,
    build_force,
    terrain_set,
    refresh_all
};

struct ConsoleCommand {
    ConsoleCommandKind kind =
        ConsoleCommandKind::local_chat;
    std::vector<std::string> arguments;
};

[[nodiscard]] ConsoleCommand parse_console_command(
    std::string_view input);

[[nodiscard]] std::string console_help();

} // namespace ogl::viewer::app
