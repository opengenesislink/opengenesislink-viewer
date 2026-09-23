#include "opengenesislink/viewer/scene/frame.hpp"
#include "opengenesislink/viewer/scene/session_protocol.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

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

void test_frame_wire_encoding() {
    const Frame frame{
        .type = MessageType::scene_sync_request,
        .request_id = 0x01020304U,
        .payload = payload_from_string("abc"),
    };
    const auto encoded = encode_frame(frame);

    require(encoded.size() == 19U, "encoded frame size mismatch");
    require(encoded[0] == std::byte{'O'} &&
            encoded[1] == std::byte{'G'} &&
            encoded[2] == std::byte{'L'} &&
            encoded[3] == std::byte{'1'}, "OGL1 magic mismatch");
    require(encoded[4] == std::byte{0x00} && encoded[5] == std::byte{0x01},
            "protocol version is not network byte order");
    require(encoded[6] == std::byte{0x00} && encoded[7] == std::byte{0x84},
            "message type is not network byte order");
    require(encoded[8] == std::byte{0x01} &&
            encoded[9] == std::byte{0x02} &&
            encoded[10] == std::byte{0x03} &&
            encoded[11] == std::byte{0x04}, "request id is not network byte order");
    require(encoded[12] == std::byte{0x00} &&
            encoded[13] == std::byte{0x00} &&
            encoded[14] == std::byte{0x00} &&
            encoded[15] == std::byte{0x03}, "payload length mismatch");

    const auto decoded = decode_frame(encoded);
    require(decoded.type == MessageType::scene_sync_request, "decoded type mismatch");
    require(decoded.request_id == 0x01020304U, "decoded request id mismatch");
    require(payload_as_string(decoded) == "abc", "decoded payload mismatch");
}

void test_frame_rejects_invalid_input() {
    auto valid = encode_frame({
        .type = MessageType::hello,
        .request_id = 1U,
        .payload = {},
    });

    auto bad_magic = valid;
    bad_magic[0] = std::byte{'X'};
    require_throws([&] { (void)decode_frame(bad_magic); },
                   "invalid magic must be rejected");

    auto bad_protocol = valid;
    bad_protocol[5] = std::byte{0x02};
    require_throws([&] { (void)decode_frame(bad_protocol); },
                   "unsupported protocol must be rejected");

    auto oversized_header = valid;
    oversized_header[12] = std::byte{0x00};
    oversized_header[13] = std::byte{0x10};
    oversized_header[14] = std::byte{0x00};
    oversized_header[15] = std::byte{0x01};
    require_throws([&] { (void)decode_frame(oversized_header); },
                   "payload larger than 1 MiB must be rejected");

    require_throws([&] {
        Frame oversized{
            .type = MessageType::scene_sync,
            .request_id = 1U,
            .payload = std::vector<std::byte>(
                static_cast<std::size_t>(kOgl1MaxPayloadSize) + 1U),
        };
        (void)encode_frame(oversized);
    }, "encoder must reject payload larger than 1 MiB");
}

void test_stream_decoder_fragmentation_and_coalescing() {
    const auto first = encode_frame({
        .type = MessageType::hello,
        .request_id = 7U,
        .payload = {},
    });
    const auto second = encode_frame({
        .type = MessageType::scene_join,
        .request_id = 8U,
        .payload = payload_from_string("region=r1\nticket=t1\n"),
    });

    FrameStreamDecoder decoder;
    const auto partial = decoder.feed(
        std::span<const std::byte>{first.data(), 5U});
    require(partial.empty(), "partial header must not emit a frame");
    require(decoder.buffered_bytes() == 5U, "fragment buffer size mismatch");

    std::vector<std::byte> remainder;
    remainder.insert(remainder.end(), first.begin() + 5, first.end());
    remainder.insert(remainder.end(), second.begin(), second.end());

    const auto frames = decoder.feed(remainder);
    require(frames.size() == 2U, "coalesced frames were not both decoded");
    require(frames[0].type == MessageType::hello &&
            frames[0].request_id == 7U, "first stream frame mismatch");
    require(frames[1].type == MessageType::scene_join &&
            frames[1].request_id == 8U, "second stream frame mismatch");
    require(decoder.buffered_bytes() == 0U, "stream decoder left trailing bytes");
}

void test_hello_contract() {
    const auto hello = make_hello(11U);
    require(hello.type == MessageType::hello, "HELLO type mismatch");
    require(hello.request_id == 11U, "HELLO request id mismatch");
    require(hello.payload.empty(), "HELLO payload must be empty");

    const std::string server_payload =
        "protocol=1\n"
        "server=opengenesis-scene\n"
        "auth=scene-ticket-v1\n"
        "scene_contract=2\n"
        "movement=avatar-reconcile-v1\n"
        "sync=scene-sync-v1\n"
        "metadata=region-metadata-v1,parcel-read-v1\n"
        "capabilities=scene-capabilities-v2\n"
        "object_runtime=linkset-v2,motion-v1\n"
        "future_additive_field=ignored\n";

    const auto ack = parse_hello_ack({
        .type = MessageType::hello_ack,
        .request_id = 11U,
        .payload = payload_from_string(server_payload),
    }, 11U);

    require(ack.protocol == 1U, "HELLO_ACK protocol mismatch");
    require(ack.scene_contract == 2U, "HELLO_ACK Scene contract mismatch");
    require(ack.server == "opengenesis-scene", "HELLO_ACK server mismatch");
    require(ack.sync == "scene-sync-v1", "HELLO_ACK sync mismatch");

    require_throws([&] {
        (void)parse_hello_ack({
            .type = MessageType::hello_ack,
            .request_id = 12U,
            .payload = payload_from_string(server_payload),
        }, 11U);
    }, "HELLO_ACK correlation mismatch must be rejected");
}

void test_scene_join_contract() {
    const auto join = make_scene_join(21U, "region-123", "signed-ticket");
    require(join.type == MessageType::scene_join, "SCENE_JOIN type mismatch");
    require(payload_as_string(join) ==
            "region=region-123\nticket=signed-ticket\n",
            "SCENE_JOIN payload mismatch");

    const std::string joined_payload =
        "status=joined\n"
        "region=region-123\n"
        "user_id=user-9\n"
        "avatar_id=42\n"
        "sequence=99\n"
        "scene_contract=2\n"
        "sync=scene-sync-v1\n"
        "movement=avatar-reconcile-v1\n"
        "terrain_revision=7\n"
        "capabilities=scene.join,scene.sync\n"
        "handoff_from=\n"
        "crossing_id=\n"
        "groups=group-a,group-b\n"
        "spawn_x=128\n"
        "spawn_y=64.5\n"
        "spawn_z=25\n";

    const auto ack = parse_scene_join_ack({
        .type = MessageType::scene_join_ack,
        .request_id = 21U,
        .payload = payload_from_string(joined_payload),
    }, 21U);

    require(ack.region_id == "region-123", "joined region mismatch");
    require(ack.user_id == "user-9", "joined user mismatch");
    require(ack.avatar_id == 42U, "avatar id mismatch");
    require(ack.sequence == 99U, "Scene sequence mismatch");
    require(ack.terrain_revision == 7U, "terrain revision mismatch");
    require(ack.spawn_y == 64.5, "spawn position mismatch");

    const auto error = parse_scene_error({
        .type = MessageType::error,
        .request_id = 21U,
        .payload = payload_from_string(
            "reason=missing-capability\ncapability=scene.sync\n"),
    });
    require(error.reason == "missing-capability", "Scene error reason mismatch");
    require(error.capability == "scene.sync", "Scene error capability mismatch");
}

void test_scene_sync_contract() {
    const auto initial = make_scene_sync_request(30U, 0U);
    require(initial.type == MessageType::scene_sync_request,
            "SCENE_SYNC_REQUEST type mismatch");
    require(payload_as_string(initial) == "since=0\nmax_events=256\n",
            "initial Scene sync payload mismatch");

    const auto delta = make_scene_sync_request(31U, 998U, 1024U);
    require(payload_as_string(delta) == "since=998\nmax_events=1024\n",
            "delta Scene sync payload mismatch");

    require_throws([&] { (void)make_scene_sync_request(32U, 0U, 0U); },
                   "max_events=0 must be rejected");
    require_throws([&] { (void)make_scene_sync_request(33U, 0U, 1025U); },
                   "max_events>1024 must be rejected");
}

} // namespace

int main() {
    try {
        test_frame_wire_encoding();
        test_frame_rejects_invalid_input();
        test_stream_decoder_fragmentation_and_coalescing();
        test_hello_contract();
        test_scene_join_contract();
        test_scene_sync_contract();
        std::cout << "OpenGenesisLINK Viewer Scene protocol tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr << "Scene protocol test failure: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}
