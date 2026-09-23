#include "opengenesislink/viewer/core/platform_client.hpp"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace ogl::viewer::core;

class FakeTransport final : public HttpTransport {
public:
    std::vector<HttpResponse> responses;
    std::vector<HttpRequest> requests;

    HttpResponse perform(
        const HttpRequest& request) override {
        requests.push_back(request);
        if (responses.empty()) {
            throw std::runtime_error(
                "FakeTransport has no queued response");
        }
        auto response =
            std::move(responses.front());
        responses.erase(responses.begin());
        return response;
    }
};

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

nlohmann::json body(
    const HttpRequest& request) {
    return request.body.empty()
        ? nlohmann::json::object()
        : nlohmann::json::parse(request.body);
}

void test_appearance_and_inventory() {
    FakeTransport transport;
    transport.responses = {
        {200, R"({
            "user_id":"u1",
            "revision":2,
            "avatar_height":1.85,
            "visual_params":"1,2",
            "wearables":[
                {"slot":"shirt","item_id":"i1","asset_id":"a1"}
            ],
            "attachments":[]
        })"},
        {200, R"({
            "user_id":"u1",
            "revision":3,
            "avatar_height":1.85,
            "visual_params":"1,2",
            "wearables":[
                {"slot":"shirt","item_id":"i2","asset_id":"a2"}
            ],
            "attachments":[]
        })"},
        {200, R"({
            "root":{"id":"root","name":"Inventory"},
            "folders":[],
            "items":[]
        })"},
        {201, R"({
            "folder":{
                "id":"f1",
                "parent_id":"root",
                "name":"Clothing"
            }
        })"},
        {201, R"({
            "item":{
                "id":"i2",
                "asset_id":"a2",
                "name":"Shirt"
            }
        })"}
    };

    PlatformClient client(transport);

    const auto appearance =
        client.appearance(
            "https://core.example/",
            "token");
    require(appearance.revision == 2U,
            "Appearance revision mismatch");

    const auto changed =
        client.set_wearable(
            "https://core.example",
            "token",
            "shirt",
            "i2",
            "a2");
    require(changed.revision == 3U,
            "Wearable response revision mismatch");
    require(
        transport.requests[1].url ==
            "https://core.example/v1/avatar/appearance/wearable",
        "Wearable endpoint mismatch");
    const auto wearable_body =
        body(transport.requests[1]);
    require(
        wearable_body.at("slot") == "shirt" &&
        wearable_body.at("item_id") == "i2" &&
        wearable_body.at("asset_id") == "a2",
        "Wearable request body mismatch");

    const auto inventory =
        client.inventory(
            "https://core.example",
            "token");
    require(inventory.root.id == "root",
            "Inventory root mismatch");

    const auto folder =
        client.create_folder(
            "https://core.example",
            "token",
            "root",
            "Clothing");
    require(folder.id == "f1",
            "Created folder id mismatch");

    const auto item =
        client.create_item(
            "https://core.example",
            "token",
            "f1",
            "a2",
            "Shirt");
    require(
        item.id == "i2" &&
        item.parent_id == "f1" &&
        item.asset_id == "a2",
        "Created Inventory item mismatch");
}

void test_social_and_notifications() {
    FakeTransport transport;
    transport.responses = {
        {200, R"({
            "friends":[
                {
                    "id":"rel1",
                    "other_user_id":"u2",
                    "status":"accepted",
                    "requested_by":"u1",
                    "created_unix":1,
                    "updated_unix":2
                }
            ]
        })"},
        {200, R"({
            "unread":1,
            "messages":[
                {
                    "id":"m1",
                    "sender_id":"u2",
                    "recipient_id":"u1",
                    "direction":"in",
                    "text":"Hello",
                    "sent_unix":3,
                    "read_unix":0
                }
            ]
        })"},
        {201, R"({"message_id":"m2","sent_unix":4})"},
        {200, R"({
            "policies":[
                {
                    "target_user_id":"u3",
                    "blocked":false,
                    "muted":true,
                    "updated_unix":5
                }
            ]
        })"},
        {200, R"({"status":"updated"})"},
        {200, R"({
            "unread":1,
            "notifications":[
                {
                    "id":"n1",
                    "type":"friend_request",
                    "title":"Friend request",
                    "body":"Request",
                    "target":"rel1",
                    "created_unix":6,
                    "read_unix":0
                }
            ]
        })"},
        {200, R"({"status":"read"})"}
    };

    PlatformClient client(transport);

    const auto friends =
        client.friends(
            "https://core.example",
            "token");
    require(
        friends.size() == 1U &&
        friends[0].other_user_id == "u2",
        "Friend parsing mismatch");

    const auto messages =
        client.messages(
            "https://core.example",
            "token");
    require(
        messages.unread == 1U &&
        messages.messages.size() == 1U &&
        messages.messages[0].text == "Hello",
        "Message parsing mismatch");

    client.send_message(
        "https://core.example",
        "token",
        "u2",
        "Reply");
    require(
        transport.requests[2].method == "POST" &&
        transport.requests[2].url.ends_with(
            "/v1/social/messages"),
        "Direct message endpoint mismatch");
    require(
        body(transport.requests[2]).at("text") ==
            "Reply",
        "Direct message body mismatch");

    const auto policies =
        client.social_policies(
            "https://core.example",
            "token");
    require(
        policies.size() == 1U &&
        policies[0].muted,
        "Social policy parsing mismatch");

    client.set_blocked(
        "https://core.example",
        "token",
        "u3",
        true);
    require(
        body(transport.requests[4]).at("blocked") ==
            true,
        "Block policy body mismatch");

    const auto notifications =
        client.notifications(
            "https://core.example",
            "token");
    require(
        notifications.unread == 1U &&
        notifications.notifications[0].id == "n1",
        "Notification parsing mismatch");

    client.mark_notification_read(
        "https://core.example",
        "token",
        "n1");
    require(
        body(transport.requests[6])
            .at("notification_id") == "n1",
        "Notification read body mismatch");
}

void test_groups_and_parcels() {
    FakeTransport transport;
    transport.responses = {
        {200, R"({
            "groups":[
                {
                    "id":"g1",
                    "name":"Builders",
                    "founder_user_id":"u1",
                    "created_unix":10
                }
            ]
        })"},
        {200, R"({
            "members":[
                {
                    "user_id":"u1",
                    "role":"owner",
                    "powers":255,
                    "joined_unix":10
                }
            ]
        })"},
        {200, R"({
            "invites":[
                {
                    "id":"inv1",
                    "group_id":"g1",
                    "invited_user_id":"u2",
                    "invited_by_user_id":"u1",
                    "role":"member",
                    "state":"pending",
                    "created_unix":11,
                    "expires_unix":20,
                    "resolved_unix":0
                }
            ]
        })"},
        {200, R"({
            "posts":[
                {
                    "id":"p1",
                    "group_id":"g1",
                    "sender_id":"u1",
                    "kind":"chat",
                    "title":"",
                    "text":"Hello group",
                    "sent_unix":12
                }
            ]
        })"},
        {200, R"({
            "parcels":[
                {
                    "id":"parcel1",
                    "region_id":"region1",
                    "name":"Home",
                    "owner_user_id":"u1",
                    "group_id":"g1",
                    "x1":0,
                    "y1":0,
                    "x2":127,
                    "y2":127,
                    "public_entry":true,
                    "public_build":false,
                    "group_build":true,
                    "group_terraform":false
                }
            ]
        })"},
        {200, R"({
            "parcel_id":"parcel1",
            "access":[
                {
                    "user_id":"u2",
                    "allowed":true,
                    "updated_unix":13
                }
            ]
        })"},
        {200, R"({"status":"updated"})"}
    };

    PlatformClient client(transport);

    const auto groups =
        client.groups(
            "https://core.example",
            "token");
    require(
        groups.size() == 1U &&
        groups[0].name == "Builders",
        "Group parsing mismatch");

    const auto members =
        client.group_members(
            "https://core.example",
            "token",
            "g1");
    require(
        members.size() == 1U &&
        members[0].powers == 255U,
        "Group member parsing mismatch");

    const auto invites =
        client.group_invites(
            "https://core.example",
            "token");
    require(
        invites.size() == 1U &&
        invites[0].state == "pending",
        "Group invite parsing mismatch");

    const auto posts =
        client.group_channel(
            "https://core.example",
            "token",
            "g1");
    require(
        posts.size() == 1U &&
        posts[0].text == "Hello group",
        "Group channel parsing mismatch");

    const auto parcels =
        client.region_parcels(
            "https://core.example",
            "token",
            "region1");
    require(
        parcels.size() == 1U &&
        parcels[0].group_build,
        "Parcel parsing mismatch");

    const auto access =
        client.parcel_access(
            "https://core.example",
            "token",
            "parcel1");
    require(
        access.size() == 1U &&
        access[0].allowed,
        "Parcel access parsing mismatch");

    client.set_parcel_access(
        "https://core.example",
        "token",
        "parcel1",
        "u3",
        false);
    const auto access_body =
        body(transport.requests[6]);
    require(
        access_body.at("parcel_id") ==
            "parcel1" &&
        access_body.at("user_id") == "u3" &&
        access_body.at("allowed") == false,
        "Parcel access update body mismatch");
}

void test_travel_and_handoff() {
    FakeTransport transport;
    transport.responses = {
        {200, R"({
            "user":{"id":"u1","display_name":"Resident"},
            "region":{"id":"r2","name":"Destination"},
            "scene_endpoint":"world.example:19100",
            "scene_ticket":"ticket-2",
            "capabilities":"scene.join,scene.sync",
            "handoff_from":"",
            "crossing_id":"",
            "groups":"g1,g2",
            "spawn":{"x":120,"y":121,"z":25},
            "expires_unix":1000
        })"},
        {200, R"({
            "user":{"id":"u1","display_name":"Resident"},
            "region":{"id":"r3","name":"East"},
            "scene_endpoint":"east.example:19100",
            "scene_ticket":"ticket-3",
            "capabilities":"scene.join,scene.sync",
            "handoff_from":"r2",
            "crossing_id":"cross1",
            "groups":"g1",
            "spawn":{"x":0.5,"y":128,"z":0},
            "expires_unix":1001
        })"},
        {200, R"({
            "id":"cross1",
            "user_id":"u1",
            "from_region":"r2",
            "to_region":"r3",
            "state":"reserved",
            "reservation_token":"reserve-token",
            "rollback_reason":null,
            "created_unix":20,
            "expires_unix":30,
            "reserved_unix":21,
            "completed_unix":0,
            "rolled_back_unix":0
        })"},
        {200, R"({
            "id":"cross1",
            "user_id":"u1",
            "from_region":"r2",
            "to_region":"r3",
            "state":"completed",
            "reservation_token":"reserve-token",
            "rollback_reason":null,
            "created_unix":20,
            "expires_unix":30,
            "reserved_unix":21,
            "completed_unix":22,
            "rolled_back_unix":0
        })"}
    };

    PlatformClient client(transport);

    const auto teleport =
        client.teleport(
            "https://core.example",
            "token",
            "r2",
            120.0,
            121.0,
            25.0);
    require(
        teleport.region_id == "r2" &&
        teleport.scene_ticket == "ticket-2" &&
        teleport.groups.size() == 2U,
        "Teleport response mismatch");

    const auto handoff =
        client.handoff(
            "https://core.example",
            "token",
            "r2",
            "r3",
            1.0,
            0.0,
            0.0);
    require(
        handoff.crossing_id == "cross1" &&
        handoff.handoff_from == "r2",
        "Handoff response mismatch");

    const auto reserved =
        client.reserve_handoff(
            "https://core.example",
            "token",
            "cross1",
            "r3");
    require(
        reserved.state == "reserved" &&
        reserved.reservation_token ==
            "reserve-token",
        "Handoff reservation mismatch");

    const auto complete =
        client.complete_handoff(
            "https://core.example",
            "token",
            "cross1",
            "r3",
            reserved.reservation_token);
    require(
        complete.state == "completed",
        "Handoff completion mismatch");

    require(
        body(transport.requests[3])
            .at("reservation_token") ==
            "reserve-token",
        "Handoff complete body mismatch");
}

} // namespace

int main() {
    try {
        test_appearance_and_inventory();
        test_social_and_notifications();
        test_groups_and_parcels();
        test_travel_and_handoff();
        std::cout
            << "OpenGenesisLINK Viewer platform client tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr
            << "Platform client test failure: "
            << ex.what()
            << "\n";
        return EXIT_FAILURE;
    }
}
