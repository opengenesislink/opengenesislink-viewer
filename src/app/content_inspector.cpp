#include "opengenesislink/viewer/app/content_inspector.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ogl::viewer::app {
namespace {

std::string trim_for_line(
    const std::string& value,
    std::size_t max_characters) {
    if (value.size() <= max_characters) {
        return value;
    }
    if (max_characters <= 3U) {
        return value.substr(0U, max_characters);
    }
    return value.substr(
               0U,
               max_characters - 3U) +
           "...";
}

std::string size_text(std::uint64_t bytes) {
    if (bytes < 1024U) {
        return std::to_string(bytes) + " B";
    }

    std::ostringstream output;
    output << std::fixed << std::setprecision(1);

    const auto kib =
        static_cast<double>(bytes) / 1024.0;
    if (kib < 1024.0) {
        output << kib << " KIB";
        return output.str();
    }

    const auto mib = kib / 1024.0;
    output << mib << " MIB";
    return output.str();
}

std::vector<std::string> appearance_lines(
    const core::BootstrapContent& content) {
    std::vector<std::string> lines;

    if (!content.appearance.has_value()) {
        lines.emplace_back(
            "NO APPEARANCE DATA IN VIEWER BOOTSTRAP");
        return lines;
    }

    const auto& appearance =
        *content.appearance;

    {
        std::ostringstream line;
        line << "REVISION " << appearance.revision
             << "  HEIGHT "
             << std::fixed << std::setprecision(2)
             << appearance.avatar_height;
        lines.push_back(line.str());
    }

    lines.push_back(
        "WEARABLES " +
        std::to_string(
            appearance.wearables.size()) +
        "  ATTACHMENTS " +
        std::to_string(
            appearance.attachments.size()));

    if (!appearance.visual_params_csv.empty()) {
        lines.push_back(
            "VISUAL PARAMS " +
            trim_for_line(
                appearance.visual_params_csv,
                74U));
    }

    if (appearance.wearables.empty() &&
        appearance.attachments.empty()) {
        lines.emplace_back(
            "NO WEARABLE OR ATTACHMENT REFERENCES");
    }

    for (const auto& wearable :
         appearance.wearables) {
        lines.push_back(
            "[WEAR] " +
            trim_for_line(wearable.slot, 14U) +
            " | ITEM " +
            trim_for_line(
                wearable.item_id,
                18U) +
            " | ASSET " +
            trim_for_line(
                wearable.asset_id,
                22U));
    }

    for (const auto& attachment :
         appearance.attachments) {
        lines.push_back(
            "[ATTACH] " +
            trim_for_line(
                attachment.point,
                12U) +
            " | ITEM " +
            trim_for_line(
                attachment.item_id,
                18U) +
            " | ASSET " +
            trim_for_line(
                attachment.asset_id,
                22U));
    }

    return lines;
}

std::vector<std::string> inventory_lines(
    const core::BootstrapContent& content) {
    std::vector<std::string> lines;

    if (!content.inventory.has_value()) {
        lines.emplace_back(
            "NO INVENTORY DATA IN VIEWER BOOTSTRAP");
        return lines;
    }

    const auto& inventory =
        *content.inventory;

    lines.push_back(
        "ROOT " +
        trim_for_line(
            inventory.root.name.empty()
                ? std::string{"INVENTORY"}
                : inventory.root.name,
            24U) +
        " | " +
        trim_for_line(
            inventory.root.id,
            34U));

    lines.push_back(
        "FOLDERS " +
        std::to_string(
            inventory.folders.size()) +
        "  ITEMS " +
        std::to_string(
            inventory.items.size()));

    for (const auto& folder :
         inventory.folders) {
        lines.push_back(
            "[FOLDER] " +
            trim_for_line(
                folder.name,
                28U) +
            " | ID " +
            trim_for_line(
                folder.id,
                26U));
    }

    for (const auto& item :
         inventory.items) {
        lines.push_back(
            "[ITEM] " +
            trim_for_line(
                item.name,
                26U) +
            " | ASSET " +
            trim_for_line(
                item.asset_id,
                28U));
    }

    return lines;
}

std::vector<std::string> asset_lines(
    const core::BootstrapContent& content) {
    std::vector<std::string> lines;

    if (content.assets.empty()) {
        lines.emplace_back(
            "NO OWNED ASSET METADATA IN VIEWER BOOTSTRAP");
        return lines;
    }

    lines.push_back(
        "OWNED ASSET METADATA " +
        std::to_string(content.assets.size()));

    for (const auto& asset :
         content.assets) {
        lines.push_back(
            "[ASSET] " +
            trim_for_line(
                asset.name.empty()
                    ? asset.id
                    : asset.name,
                24U) +
            " | " +
            trim_for_line(
                asset.mime_type.empty()
                    ? std::string{"UNKNOWN MIME"}
                    : asset.mime_type,
                22U) +
            " | " +
            size_text(asset.size));

        lines.push_back(
            "  ID " +
            trim_for_line(
                asset.id,
                28U) +
            " | HASH " +
            trim_for_line(
                asset.content_hash.empty()
                    ? std::string{"NONE"}
                    : asset.content_hash,
                28U));
    }

    return lines;
}

std::string title_for(
    ContentInspectorSection section) {
    switch (section) {
    case ContentInspectorSection::appearance:
        return "APPEARANCE";
    case ContentInspectorSection::inventory:
        return "INVENTORY";
    case ContentInspectorSection::assets:
        return "ASSETS";
    }
    return "CONTENT";
}

std::vector<std::string> all_lines(
    const core::BootstrapContent& content,
    ContentInspectorSection section) {
    switch (section) {
    case ContentInspectorSection::appearance:
        return appearance_lines(content);
    case ContentInspectorSection::inventory:
        return inventory_lines(content);
    case ContentInspectorSection::assets:
        return asset_lines(content);
    }
    return {};
}

} // namespace

ContentInspectorSection
next_content_inspector_section(
    ContentInspectorSection section) noexcept {
    switch (section) {
    case ContentInspectorSection::appearance:
        return ContentInspectorSection::inventory;
    case ContentInspectorSection::inventory:
        return ContentInspectorSection::assets;
    case ContentInspectorSection::assets:
        return ContentInspectorSection::appearance;
    }
    return ContentInspectorSection::appearance;
}

ContentInspectorView build_content_inspector_view(
    const core::BootstrapContent& content,
    ContentInspectorSection section,
    std::size_t requested_page,
    std::size_t page_size) {
    if (page_size == 0U) {
        throw std::invalid_argument(
            "Content inspector page size must be non-zero");
    }

    auto lines =
        all_lines(content, section);

    const auto page_count =
        std::max<std::size_t>(
            1U,
            (lines.size() + page_size - 1U) /
                page_size);
    const auto page =
        std::min(
            requested_page,
            page_count - 1U);
    const auto begin =
        std::min(
            page * page_size,
            lines.size());
    const auto end =
        std::min(
            begin + page_size,
            lines.size());

    ContentInspectorView view;
    view.section = section;
    view.title = title_for(section);
    view.page = page;
    view.page_count = page_count;
    view.lines.assign(
        lines.begin() +
            static_cast<std::ptrdiff_t>(begin),
        lines.begin() +
            static_cast<std::ptrdiff_t>(end));
    return view;
}

} // namespace ogl::viewer::app
