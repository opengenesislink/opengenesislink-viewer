#include "opengenesislink/viewer/core/bootstrap_content.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace ogl::viewer::core {
namespace {

std::string string_value(
    const nlohmann::json& value,
    const char* key) {
    if (!value.is_object()) {
        return {};
    }
    const auto found = value.find(key);
    return found != value.end() && found->is_string()
        ? found->get<std::string>()
        : std::string{};
}

std::uint64_t u64_value(
    const nlohmann::json& value,
    const char* key) {
    if (!value.is_object()) {
        return 0U;
    }
    const auto found = value.find(key);
    if (found == value.end() ||
        !found->is_number_unsigned()) {
        if (found != value.end() &&
            found->is_number_integer()) {
            const auto signed_value =
                found->get<std::int64_t>();
            return signed_value >= 0
                ? static_cast<std::uint64_t>(signed_value)
                : 0U;
        }
        return 0U;
    }
    return found->get<std::uint64_t>();
}

std::int64_t i64_value(
    const nlohmann::json& value,
    const char* key) {
    if (!value.is_object()) {
        return 0;
    }
    const auto found = value.find(key);
    return found != value.end() &&
           found->is_number_integer()
        ? found->get<std::int64_t>()
        : 0;
}

std::uint32_t u32_value(
    const nlohmann::json& value,
    const char* key) {
    const auto parsed = u64_value(value, key);
    return parsed <=
            static_cast<std::uint64_t>(
                std::numeric_limits<std::uint32_t>::max())
        ? static_cast<std::uint32_t>(parsed)
        : 0U;
}

AvatarAppearanceSnapshot parse_appearance(
    const nlohmann::json& value) {
    AvatarAppearanceSnapshot result;
    result.user_id =
        string_value(value, "user_id");
    result.revision =
        u64_value(value, "revision");
    result.avatar_height =
        value.value("avatar_height", 1.9);
    result.visual_params_csv =
        string_value(value, "visual_params");
    result.updated_unix =
        i64_value(value, "updated_unix");

    if (const auto wearables =
            value.find("wearables");
        wearables != value.end() &&
        wearables->is_array()) {
        result.wearables.reserve(
            wearables->size());
        for (const auto& entry : *wearables) {
            if (!entry.is_object()) {
                continue;
            }
            WearableRef wearable{
                .slot =
                    string_value(entry, "slot"),
                .item_id =
                    string_value(entry, "item_id"),
                .asset_id =
                    string_value(entry, "asset_id"),
            };
            if (!wearable.slot.empty() ||
                !wearable.item_id.empty() ||
                !wearable.asset_id.empty()) {
                result.wearables.push_back(
                    std::move(wearable));
            }
        }
    }

    if (const auto attachments =
            value.find("attachments");
        attachments != value.end() &&
        attachments->is_array()) {
        result.attachments.reserve(
            attachments->size());
        for (const auto& entry : *attachments) {
            if (!entry.is_object()) {
                continue;
            }
            AttachmentRef attachment{
                .point =
                    string_value(entry, "point"),
                .item_id =
                    string_value(entry, "item_id"),
                .asset_id =
                    string_value(entry, "asset_id"),
            };
            if (!attachment.point.empty() ||
                !attachment.item_id.empty() ||
                !attachment.asset_id.empty()) {
                result.attachments.push_back(
                    std::move(attachment));
            }
        }
    }

    return result;
}

InventorySnapshot parse_inventory(
    const nlohmann::json& value) {
    InventorySnapshot result;

    if (const auto root = value.find("root");
        root != value.end() &&
        root->is_object()) {
        result.root.id =
            string_value(*root, "id");
        result.root.name =
            string_value(*root, "name");
    }

    if (const auto folders =
            value.find("folders");
        folders != value.end() &&
        folders->is_array()) {
        result.folders.reserve(
            folders->size());
        for (const auto& entry : *folders) {
            if (!entry.is_object()) {
                continue;
            }
            InventoryFolderSnapshot folder{
                .id =
                    string_value(entry, "id"),
                .parent_id =
                    string_value(entry, "parent_id"),
                .name =
                    string_value(entry, "name"),
            };
            if (!folder.id.empty()) {
                result.folders.push_back(
                    std::move(folder));
            }
        }
    }

    if (const auto items =
            value.find("items");
        items != value.end() &&
        items->is_array()) {
        result.items.reserve(items->size());
        for (const auto& entry : *items) {
            if (!entry.is_object()) {
                continue;
            }
            InventoryItemSnapshot item{
                .id =
                    string_value(entry, "id"),
                .parent_id =
                    string_value(entry, "parent_id"),
                .asset_id =
                    string_value(entry, "asset_id"),
                .name =
                    string_value(entry, "name"),
            };
            if (!item.id.empty()) {
                result.items.push_back(
                    std::move(item));
            }
        }
    }

    return result;
}

AssetMetadata parse_asset(
    const nlohmann::json& value) {
    return {
        .id = string_value(value, "id"),
        .name = string_value(value, "name"),
        .mime_type =
            string_value(value, "mime_type"),
        .content_hash =
            string_value(value, "content_hash"),
        .size = u64_value(value, "size"),
        .permissions =
            u32_value(value, "permissions"),
        .next_owner_permissions =
            u32_value(
                value,
                "next_owner_permissions"),
        .created_unix =
            i64_value(value, "created_unix"),
    };
}

} // namespace

BootstrapContent parse_bootstrap_content(
    std::string_view json) {
    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(json);
    } catch (const nlohmann::json::exception& ex) {
        throw std::runtime_error(
            std::string{
                "Viewer bootstrap content is invalid JSON: "} +
            ex.what());
    }

    if (!doc.is_object()) {
        throw std::runtime_error(
            "Viewer bootstrap content must be an object");
    }

    BootstrapContent result;

    if (const auto appearance =
            doc.find("appearance");
        appearance != doc.end() &&
        appearance->is_object()) {
        result.appearance =
            parse_appearance(*appearance);
    }

    if (const auto inventory =
            doc.find("inventory");
        inventory != doc.end() &&
        inventory->is_object()) {
        result.inventory =
            parse_inventory(*inventory);
    }

    if (const auto assets = doc.find("assets");
        assets != doc.end() &&
        assets->is_array()) {
        result.assets.reserve(assets->size());
        for (const auto& entry : *assets) {
            if (!entry.is_object()) {
                continue;
            }
            auto asset = parse_asset(entry);
            if (!asset.id.empty()) {
                result.assets.push_back(
                    std::move(asset));
            }
        }
    }

    if (const auto endpoints =
            doc.find("endpoints");
        endpoints != doc.end() &&
        endpoints->is_object()) {
        for (auto it = endpoints->begin();
             it != endpoints->end();
             ++it) {
            if (it.value().is_string()) {
                result.endpoints.emplace(
                    it.key(),
                    it.value().get<std::string>());
            }
        }
    }

    return result;
}

std::vector<std::string>
appearance_asset_dependencies(
    const AvatarAppearanceSnapshot& appearance) {
    std::vector<std::string> result;
    std::unordered_set<std::string> seen;

    const auto add =
        [&result, &seen](
            const std::string& asset_id) {
            if (!asset_id.empty() &&
                seen.insert(asset_id).second) {
                result.push_back(asset_id);
            }
        };

    for (const auto& wearable :
         appearance.wearables) {
        add(wearable.asset_id);
    }
    for (const auto& attachment :
         appearance.attachments) {
        add(attachment.asset_id);
    }
    return result;
}

const AssetMetadata* find_asset_metadata(
    const BootstrapContent& content,
    std::string_view asset_id) noexcept {
    const auto found = std::find_if(
        content.assets.begin(),
        content.assets.end(),
        [asset_id](const AssetMetadata& asset) {
            return asset.id == asset_id;
        });
    return found != content.assets.end()
        ? &(*found)
        : nullptr;
}

} // namespace ogl::viewer::core
