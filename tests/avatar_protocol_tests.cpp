#include "opengenesislink/viewer/scene/avatar_protocol.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

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

void test_reconcile_request_encoding() {
    const AvatarReconcileRequest request{
        .client_sequence = 7U,
        .pose = {
            .x = 130.0,
            .y = 131.0,
            .z = 25.0,
            .rx = 0.0,
            .ry = 0.0,
            .rz = 45.0,
        },
        .velocity = {
            .x = 1.0,
            .y = 0.0,
            .z = 0.0,
        },
    };

    const auto frame =
        make_avatar_reconcile(42U, request);

    require(frame.type == MessageType::avatar_reconcile,
            "avatar reconcile message type mismatch");
    require(frame.request_id == 42U,
            "avatar reconcile request id mismatch");
    require(
        payload_as_string(frame) ==
            "client_sequence=7\n"
            "x=130\n"
            "y=131\n"
            "z=25\n"
            "rx=0\n"
            "ry=0\n"
            "rz=45\n"
            "vx=1\n"
            "vy=0\n"
            "vz=0\n",
        "avatar reconcile payload mismatch");

    require_throws(
        [] {
            AvatarReconcileRequest invalid;
            (void)make_avatar_reconcile(1U, invalid);
        },
        "zero client sequence must fail");
}

void test_reconcile_ack_parsing() {
    const Frame frame{
        .type = MessageType::avatar_reconcile_ack,
        .request_id = 77U,
        .payload = payload_from_string(
            "status=reconciled\n"
            "client_sequence=7\n"
            "server_sequence=99\n"
            "tick=1234\n"
            "boundary=east\n"
            "x=130.5\n"
            "y=131.25\n"
            "z=25\n"
            "rx=0\n"
            "ry=0\n"
            "rz=45\n"
            "vx=1.5\n"
            "vy=0\n"
            "vz=-0.25\n"),
    };

    const auto ack =
        parse_avatar_reconcile_ack(frame, 77U);

    require(ack.client_sequence == 7U,
            "reconcile ack client sequence mismatch");
    require(ack.server_sequence == 99U,
            "reconcile ack server sequence mismatch");
    require(ack.tick == 1234U,
            "reconcile ack tick mismatch");
    require(ack.boundary == "east",
            "reconcile ack boundary mismatch");
    require(ack.pose.x == 130.5 &&
            ack.pose.y == 131.25 &&
            ack.pose.rz == 45.0,
            "reconcile ack pose mismatch");
    require(ack.velocity.x == 1.5 &&
            ack.velocity.z == -0.25,
            "reconcile ack velocity mismatch");
}

void test_reconcile_error_propagation() {
    const Frame error{
        .type = MessageType::error,
        .request_id = 3U,
        .payload = payload_from_string(
            "reason=stale-client-sequence\n"),
    };

    require_throws(
        [&] {
            (void)parse_avatar_reconcile_ack(
                error,
                3U);
        },
        "Scene error must fail avatar reconcile parsing");
}

} // namespace

int main() {
    try {
        test_reconcile_request_encoding();
        test_reconcile_ack_parsing();
        test_reconcile_error_propagation();
        std::cout
            << "OpenGenesisLINK Viewer avatar protocol tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr
            << "Avatar protocol test failure: "
            << ex.what()
            << "\n";
        return EXIT_FAILURE;
    }
}
