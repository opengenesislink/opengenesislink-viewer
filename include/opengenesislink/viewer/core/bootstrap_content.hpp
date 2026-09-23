#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ogl::viewer::core {

struct WearableRef {
    std::string slot;
    std::string item_id;
    std::string asset_id;
};

struct AttachmentRef {
    std::string point;
    std::string item_id;
    std::string asset_id;
};

struct AvatarAppearanceSnapshot {
    std::string user_id;
    std::uint64_t revision = 0;
    double avatar_height = 1.9;
    std::string visual_params_csv;
    std::int64_t updated_unix = 0;
    std::vector<WearableRef> wearables;
    std::vector<AttachmentRef> attachments;
};

struct InventoryRootSnapshot {
    std::string id;
    std::string name;
};

struct InventoryFolderSnapshot {
    std::string id;
    std::string parent_id;
    std::string name;
};

struct InventoryItemSnapshot {
    std::string id;
    std::string parent_id;
    std::string asset_id;
    std::string name;
};

struct InventorySnapshot {
    InventoryRootSnapshot root;
    std::vector<InventoryFolderSnapshot> folders;
    std::vector<InventoryItemSnapshot> items;
};

struct AssetMetadata {
    std::string id;
    std::string name;
    std::string mime_type;
    std::string content_hash;
    std::uint64_t size = 0;
    std::uint32_t permissions = 0;
    std::uint32_t next_owner_permissions = 0;
    std::int64_t created_unix = 0;
};

struct BootstrapContent {
    std::optional<AvatarAppearanceSnapshot> appearance;
    std::optional<InventorySnapshot> inventory;
    std::vector<AssetMetadata> assets;
    std::map<std::string, std::string> endpoints;
};

[[nodiscard]] AvatarAppearanceSnapshot parse_appearance_snapshot(
    std::string_view json);

[[nodiscard]] InventorySnapshot parse_inventory_snapshot(
    std::string_view json);

[[nodiscard]] BootstrapContent parse_bootstrap_content(
    std::string_view json);

[[nodiscard]] std::vector<std::string>
appearance_asset_dependencies(
    const AvatarAppearanceSnapshot& appearance);

[[nodiscard]] const AssetMetadata* find_asset_metadata(
    const BootstrapContent& content,
    std::string_view asset_id) noexcept;

} // namespace ogl::viewer::core
