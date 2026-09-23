#include "opengenesislink/viewer/scene/world_actions.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace ogl::viewer::scene;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <typename Function>
void require_throws(Function&& function, const char* message) {
    bool threw = false;
    try {
        function();
    } catch (const std::exception&) {
        threw = true;
    }
    require(threw, message);
}

void test_chat_and_object_payloads() {
    const auto chat = make_chat_send(1U, "Hello World");
    require(chat.type == MessageType::chat_send,
            "chat message type mismatch");
    require(payload_as_string(chat) ==
                "text=Hello World\n",
            "chat payload mismatch");

    ObjectCreateRequest create;
    create.name = "Cube";
    create.transform = {
        .x = 10.0,
        .y = 20.0,
        .z = 30.0,
        .rx = 1.0,
        .ry = 2.0,
        .rz = 3.0,
        .sx = 4.0,
        .sy = 5.0,
        .sz = 6.0,
    };
    create.physical = true;
    create.group_id = "group-1";
    create.group_permissions = 7U;
    create.everyone_permissions = 1U;

    const auto frame =
        make_object_create(2U, create);
    const auto payload =
        payload_as_string(frame);

    require(frame.type == MessageType::entity_create,
            "object create type mismatch");
    require(payload.find("name=Cube\n") !=
                std::string::npos,
            "object create name missing");
    require(payload.find("x=10\n") !=
                std::string::npos,
            "object create x missing");
    require(payload.find("physical=true\n") !=
                std::string::npos,
            "object create physical flag missing");
    require(payload.find("group_permissions=7\n") !=
                std::string::npos,
            "object create group permissions missing");

    const auto update =
        make_object_update(
            3U,
            99U,
            create.transform);
    require(update.type == MessageType::entity_update,
            "object update type mismatch");
    require(payload_as_string(update).starts_with(
                "id=99\n"),
            "object update id missing");

    const auto deletion =
        make_object_delete(4U, 99U);
    require(payload_as_string(deletion) ==
                "id=99\n",
            "object delete payload mismatch");

    require_throws(
        [] {
            (void)make_chat_send(1U, "");
        },
        "empty chat must fail");
    require_throws(
        [] {
            (void)make_object_delete(1U, 0U);
        },
        "zero object id must fail");
}

void test_permissions_link_text_motion() {
    const auto permissions =
        make_object_permissions(
            5U,
            {
                .entity_id = 100U,
                .group_id = "group-a",
                .group_permissions = 3U,
                .everyone_permissions = 1U,
            });
    require(
        payload_as_string(permissions) ==
            "id=100\n"
            "group_id=group-a\n"
            "group_permissions=3\n"
            "everyone_permissions=1\n",
        "permissions payload mismatch");

    const auto link =
        make_object_link(
            6U,
            100U,
            101U,
            false);
    require(
        payload_as_string(link) ==
            "action=link\n"
            "root_id=100\n"
            "child_id=101\n",
        "link payload mismatch");

    const auto unlink =
        make_object_link(
            7U,
            100U,
            101U,
            true);
    require(
        payload_as_string(unlink).starts_with(
            "action=unlink\n"),
        "unlink payload mismatch");

    const auto text =
        make_object_text(
            8U,
            100U,
            "Floating");
    require(
        payload_as_string(text) ==
            "id=100\ntext=Floating\n",
        "floating text payload mismatch");

    const auto motion =
        make_object_motion(
            9U,
            {
                .entity_id = 100U,
                .vx = 1.0,
                .vy = 2.0,
                .vz = 3.0,
                .avx = 4.0,
                .avy = 5.0,
                .avz = 6.0,
            });
    const auto motion_payload =
        payload_as_string(motion);
    require(
        motion_payload.find("vx=1\n") !=
            std::string::npos &&
        motion_payload.find("avz=6\n") !=
            std::string::npos,
        "motion payload mismatch");
}

void test_physics_interaction_and_terrain() {
    const auto physics =
        make_object_physics(
            10U,
            88U,
            "material",
            "mass=2\nrestitution=0.2\nfriction=0.7\n");
    require(
        payload_as_string(physics) ==
            "id=88\n"
            "action=material\n"
            "mass=2\n"
            "restitution=0.2\n"
            "friction=0.7\n",
        "physics payload mismatch");

    const auto interact =
        make_object_interact(
            11U,
            88U,
            "touch");
    require(
        payload_as_string(interact) ==
            "id=88\nphase=touch\n",
        "interaction payload mismatch");

    const auto terrain =
        make_terrain_set_request(
            12U,
            5U,
            6U,
            21.5);
    require(
        payload_as_string(terrain) ==
            "x=5\ny=6\nheight=21.5\n",
        "terrain set payload mismatch");

    require_throws(
        [] {
            (void)make_object_interact(
                1U,
                1U,
                "invalid");
        },
        "invalid touch phase must fail");
}

void test_command_ack() {
    const auto ack =
        parse_scene_command_ack(
            {
                .type =
                    MessageType::entity_create_ack,
                .request_id = 30U,
                .payload = payload_from_string(
                    "status=created\n"
                    "id=321\n"
                    "sequence=99\n"
                    "revision=7\n"),
            },
            MessageType::entity_create_ack,
            30U);

    require(ack.status == "created",
            "command ack status mismatch");
    require(ack.entity_id == 321U,
            "command ack entity id mismatch");
    require(ack.sequence == 99U,
            "command ack sequence mismatch");
    require(ack.revision == 7U,
            "command ack revision mismatch");
}

void test_parcel_region_and_point_response() {
    const auto region =
        parse_parcel_info(
            {
                .type = MessageType::parcel_info,
                .request_id = 40U,
                .payload = payload_from_string(
                    "mode=region\n"
                    "count=2\n"
                    "parcel=p1|Main|owner-1|group-1|0|0|127|255|1|1|1|0\n"
                    "parcel=p2|East|owner-2||128|0|255|255|1|0|0|0\n"),
            },
            40U);

    require(region.mode == "region",
            "parcel region mode mismatch");
    require(region.parcels.size() == 2U,
            "parcel region count mismatch");
    require(region.parcels[0].name == "Main",
            "parcel name mismatch");
    require(region.parcels[0].public_build,
            "parcel public build mismatch");
    require(region.parcels[1].x1 == 128U,
            "parcel bounds mismatch");

    const auto point_request =
        make_parcel_info_request(
            41U,
            128.0,
            64.0);
    require(
        payload_as_string(point_request) ==
            "x=128\ny=64\n",
        "parcel point request mismatch");

    require_throws(
        [] {
            (void)make_parcel_info_request(
                1U,
                10.0,
                std::nullopt);
        },
        "partial parcel point request must fail");
}

} // namespace

int main() {
    try {
        test_chat_and_object_payloads();
        test_permissions_link_text_motion();
        test_physics_interaction_and_terrain();
        test_command_ack();
        test_parcel_region_and_point_response();
        std::cout
            << "OpenGenesisLINK Viewer world action protocol tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr
            << "World action test failure: "
            << ex.what()
            << "\n";
        return EXIT_FAILURE;
    }
}
