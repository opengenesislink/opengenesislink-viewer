#include "opengenesislink/viewer/app/content_inspector.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace ogl::viewer;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

core::BootstrapContent fixture() {
    core::BootstrapContent content;

    core::AvatarAppearanceSnapshot appearance;
    appearance.user_id = "user-1";
    appearance.revision = 8U;
    appearance.avatar_height = 1.87;
    appearance.visual_params_csv = "1,2,3,4";
    appearance.wearables.push_back({
        .slot = "shirt",
        .item_id = "item-shirt",
        .asset_id = "asset-shirt",
    });
    appearance.attachments.push_back({
        .point = "right_hand",
        .item_id = "item-tool",
        .asset_id = "asset-tool",
    });
    content.appearance = appearance;

    core::InventorySnapshot inventory;
    inventory.root = {
        .id = "root-1",
        .name = "Inventory",
    };
    inventory.folders.push_back({
        .id = "folder-1",
        .parent_id = "root-1",
        .name = "Clothing",
    });
    inventory.items.push_back({
        .id = "item-shirt",
        .parent_id = "folder-1",
        .asset_id = "asset-shirt",
        .name = "Blue Shirt",
    });
    content.inventory = inventory;

    content.assets.push_back({
        .id = "asset-shirt",
        .name = "Blue Shirt",
        .mime_type =
            "application/octet-stream",
        .content_hash = "hash-shirt",
        .size = 1536U,
        .permissions = 31U,
        .next_owner_permissions = 6U,
        .created_unix = 100,
    });

    return content;
}

void test_section_cycle() {
    auto section =
        app::ContentInspectorSection::appearance;
    section =
        app::next_content_inspector_section(
            section);
    require(
        section ==
            app::ContentInspectorSection::inventory,
        "appearance did not advance to inventory");

    section =
        app::next_content_inspector_section(
            section);
    require(
        section ==
            app::ContentInspectorSection::assets,
        "inventory did not advance to assets");

    section =
        app::next_content_inspector_section(
            section);
    require(
        section ==
            app::ContentInspectorSection::appearance,
        "assets did not wrap to appearance");
}

void test_appearance_view() {
    const auto view =
        app::build_content_inspector_view(
            fixture(),
            app::ContentInspectorSection::appearance,
            0U,
            12U);

    require(view.title == "APPEARANCE",
            "Appearance inspector title mismatch");
    require(view.page == 0U &&
            view.page_count == 1U,
            "Appearance inspector paging mismatch");

    std::string combined;
    for (const auto& line : view.lines) {
        combined += line;
        combined += '\n';
    }

    require(
        combined.find("REVISION 8") !=
            std::string::npos,
        "Appearance revision missing");
    require(
        combined.find("[WEAR]") !=
            std::string::npos,
        "Wearable row missing");
    require(
        combined.find("[ATTACH]") !=
            std::string::npos,
        "Attachment row missing");
}

void test_inventory_view() {
    const auto view =
        app::build_content_inspector_view(
            fixture(),
            app::ContentInspectorSection::inventory,
            0U,
            12U);

    std::string combined;
    for (const auto& line : view.lines) {
        combined += line;
        combined += '\n';
    }

    require(
        combined.find("[FOLDER]") !=
            std::string::npos,
        "Inventory folder row missing");
    require(
        combined.find("[ITEM]") !=
            std::string::npos,
        "Inventory item row missing");
    require(
        combined.find("asset-shirt") !=
            std::string::npos,
        "Inventory Asset reference missing");
}

void test_asset_view_and_paging() {
    auto content = fixture();
    for (std::size_t index = 0U;
         index < 8U;
         ++index) {
        content.assets.push_back({
            .id =
                "asset-" +
                std::to_string(index),
            .name =
                "Asset " +
                std::to_string(index),
            .mime_type = "text/plain",
            .content_hash =
                "hash-" +
                std::to_string(index),
            .size =
                static_cast<std::uint64_t>(
                    100U + index),
        });
    }

    const auto first =
        app::build_content_inspector_view(
            content,
            app::ContentInspectorSection::assets,
            0U,
            5U);
    require(first.page_count > 1U,
            "Asset inspector did not paginate");
    require(first.lines.size() == 5U,
            "Asset inspector first page size mismatch");

    const auto clamped =
        app::build_content_inspector_view(
            content,
            app::ContentInspectorSection::assets,
            999U,
            5U);
    require(
        clamped.page ==
            clamped.page_count - 1U,
        "Asset inspector page was not clamped");
    require(!clamped.lines.empty(),
            "Asset inspector final page is empty");
}

void test_missing_sections() {
    core::BootstrapContent empty;

    const auto appearance =
        app::build_content_inspector_view(
            empty,
            app::ContentInspectorSection::appearance,
            0U,
            10U);
    require(
        appearance.lines.size() == 1U,
        "Missing Appearance placeholder mismatch");

    const auto inventory =
        app::build_content_inspector_view(
            empty,
            app::ContentInspectorSection::inventory,
            0U,
            10U);
    require(
        inventory.lines.size() == 1U,
        "Missing Inventory placeholder mismatch");

    const auto assets =
        app::build_content_inspector_view(
            empty,
            app::ContentInspectorSection::assets,
            0U,
            10U);
    require(
        assets.lines.size() == 1U,
        "Missing Asset placeholder mismatch");
}

} // namespace

int main() {
    try {
        test_section_cycle();
        test_appearance_view();
        test_inventory_view();
        test_asset_view_and_paging();
        test_missing_sections();
        std::cout
            << "OpenGenesisLINK Viewer content inspector tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr
            << "Content inspector test failure: "
            << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}
