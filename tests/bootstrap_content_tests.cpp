#include "opengenesislink/viewer/core/bootstrap_content.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace ogl::viewer::core;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void test_current_bootstrap_content() {
    const auto content = parse_bootstrap_content(R"({
        "appearance":{
            "user_id":"u-1",
            "revision":7,
            "avatar_height":1.82,
            "visual_params":"1,2,3",
            "updated_unix":200,
            "wearables":[
                {"slot":"shirt","item_id":"item-1","asset_id":"asset-a"},
                {"slot":"pants","item_id":"item-2","asset_id":"asset-b"}
            ],
            "attachments":[
                {"point":"right_hand","item_id":"item-3","asset_id":"asset-c"},
                {"point":"chest","item_id":"item-4","asset_id":"asset-a"}
            ]
        },
        "inventory":{
            "root":{"id":"root","name":"Inventory"},
            "folders":[
                {"id":"folder-1","parent_id":"root","name":"Clothing"}
            ],
            "items":[
                {"id":"item-1","parent_id":"folder-1","asset_id":"asset-a","name":"Shirt"},
                {"id":"item-2","parent_id":"folder-1","asset_id":"asset-b","name":"Pants"}
            ]
        },
        "assets":[
            {
                "id":"asset-a",
                "name":"Shirt",
                "mime_type":"application/x-opengenesis-wearable",
                "size":12,
                "permissions":31,
                "next_owner_permissions":6,
                "created_unix":100,
                "content_hash":"aaa"
            },
            {
                "id":"asset-b",
                "name":"Pants",
                "mime_type":"application/x-opengenesis-wearable",
                "size":13,
                "permissions":31,
                "next_owner_permissions":6,
                "created_unix":101,
                "content_hash":"bbb"
            }
        ],
        "endpoints":{
            "asset":"GET /v1/assets/{id}",
            "inventory":"GET /v1/inventory"
        }
    })");

    require(content.appearance.has_value(),
            "appearance was not parsed");
    require(content.appearance->revision == 7U,
            "appearance revision mismatch");
    require(content.appearance->wearables.size() == 2U,
            "wearable count mismatch");
    require(content.appearance->attachments.size() == 2U,
            "attachment count mismatch");

    const auto dependencies =
        appearance_asset_dependencies(
            *content.appearance);
    require(dependencies.size() == 3U,
            "appearance dependencies were not deduplicated");
    require(dependencies[0] == "asset-a" &&
            dependencies[1] == "asset-b" &&
            dependencies[2] == "asset-c",
            "appearance dependency order mismatch");

    require(content.inventory.has_value(),
            "inventory was not parsed");
    require(content.inventory->root.id == "root",
            "inventory root mismatch");
    require(content.inventory->folders.size() == 1U,
            "inventory folder count mismatch");
    require(content.inventory->items.size() == 2U,
            "inventory item count mismatch");

    require(content.assets.size() == 2U,
            "Asset metadata count mismatch");
    const auto* asset =
        find_asset_metadata(content, "asset-a");
    require(asset != nullptr,
            "Asset metadata lookup failed");
    require(asset->content_hash == "aaa" &&
            asset->permissions == 31U,
            "Asset metadata fields mismatch");

    require(content.endpoints.at("asset") ==
                "GET /v1/assets/{id}",
            "bootstrap endpoint metadata mismatch");
}

void test_additive_and_missing_content() {
    const auto content = parse_bootstrap_content(R"({
        "future":{"anything":true},
        "assets":[
            {"future_only":"ignored"},
            {"id":"asset-a","unknown":42}
        ]
    })");

    require(!content.appearance.has_value(),
            "missing appearance should remain optional");
    require(!content.inventory.has_value(),
            "missing inventory should remain optional");
    require(content.assets.size() == 1U &&
            content.assets[0].id == "asset-a",
            "additive Asset parsing mismatch");
}

} // namespace

int main() {
    try {
        test_current_bootstrap_content();
        test_additive_and_missing_content();
        std::cout
            << "OpenGenesisLINK Viewer bootstrap content tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr
            << "Bootstrap content test failure: "
            << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}
