#include "opengenesislink/viewer/scene/scene_session.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
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

class FakeByteStream final : public ByteStream {
public:
    bool open = true;
    std::vector<Frame> sent;

    void connect(const SceneEndpoint&) override {
        open = true;
    }

    void send_all(std::span<const std::byte> bytes) override {
        const auto frame = decode_frame(bytes);
        sent.push_back(frame);

        switch (frame.type) {
        case MessageType::hello:
            queue({
                .type = MessageType::hello_ack,
                .request_id = frame.request_id,
                .payload = payload_from_string(
                    "protocol=1\n"
                    "server=opengenesis-scene\n"
                    "auth=scene-ticket-v1\n"
                    "scene_contract=2\n"
                    "movement=avatar-reconcile-v1\n"
                    "sync=scene-sync-v1\n"
                    "metadata=region-metadata-v1,parcel-read-v1\n"
                    "capabilities=scene-capabilities-v2\n"
                    "object_runtime=linkset-v2,physics-v3\n"),
            });
            break;
        case MessageType::scene_join:
            queue({
                .type = MessageType::scene_join_ack,
                .request_id = frame.request_id,
                .payload = payload_from_string(
                    "status=joined\n"
                    "region=region-1\n"
                    "user_id=user-1\n"
                    "avatar_id=77\n"
                    "sequence=123\n"
                    "scene_contract=2\n"
                    "sync=scene-sync-v1\n"
                    "movement=avatar-reconcile-v1\n"
                    "terrain_revision=4\n"
                    "capabilities=scene.join,scene.sync,scene.read\n"
                    "handoff_from=\n"
                    "crossing_id=\n"
                    "groups=\n"
                    "spawn_x=128\n"
                    "spawn_y=128\n"
                    "spawn_z=25\n"),
            });
            break;
        case MessageType::scene_sync_request:
            queue({
                .type = MessageType::chat_event,
                .request_id = 0U,
                .payload = payload_from_string("message=deferred\n"),
            });
            queue({
                .type = MessageType::scene_sync,
                .request_id = frame.request_id,
                .payload = payload_from_string(
                    "mode=snapshot\nsequence=123\nentity_count=0\n"),
            });
            break;
        case MessageType::avatar_reconcile:
            queue({
                .type = MessageType::avatar_reconcile_ack,
                .request_id = frame.request_id,
                .payload = payload_from_string(
                    "status=reconciled\n"
                    "client_sequence=5\n"
                    "server_sequence=130\n"
                    "tick=900\n"
                    "boundary=\n"
                    "x=129\n"
                    "y=130\n"
                    "z=25\n"
                    "rx=0\n"
                    "ry=0\n"
                    "rz=90\n"
                    "vx=0\n"
                    "vy=4\n"
                    "vz=0\n"),
            });
            break;
        case MessageType::goodbye:
            break;
        default:
            throw std::runtime_error("unexpected test frame type");
        }
    }

    std::size_t receive_some(std::span<std::byte> buffer) override {
        if (!open) {
            throw std::runtime_error("fake stream is closed");
        }
        if (incoming.empty()) {
            throw std::runtime_error("fake stream has no incoming bytes");
        }

        const auto count = std::min<std::size_t>(
            {buffer.size(), incoming.size(), 7U});
        for (std::size_t i = 0; i < count; ++i) {
            buffer[i] = incoming.front();
            incoming.pop_front();
        }
        return count;
    }

    void close() noexcept override {
        open = false;
        incoming.clear();
    }

    bool is_open() const noexcept override {
        return open;
    }

private:
    std::deque<std::byte> incoming;

    void queue(const Frame& frame) {
        const auto bytes = encode_frame(frame);
        incoming.insert(incoming.end(), bytes.begin(), bytes.end());
    }
};

void test_endpoint_parser() {
    const auto ipv4 = parse_scene_endpoint("world.example:19100");
    require(ipv4.host == "world.example", "hostname endpoint mismatch");
    require(ipv4.port == 19100U, "hostname port mismatch");

    const auto prefixed = parse_scene_endpoint("tcp://127.0.0.1:19101");
    require(prefixed.host == "127.0.0.1", "tcp prefix host mismatch");
    require(prefixed.port == 19101U, "tcp prefix port mismatch");

    const auto ipv6 = parse_scene_endpoint("[2001:db8::1]:19102");
    require(ipv6.host == "2001:db8::1", "IPv6 host mismatch");
    require(ipv6.port == 19102U, "IPv6 port mismatch");

    require_throws(
        [] { (void)parse_scene_endpoint("world.example"); },
        "endpoint without port must fail");
    require_throws(
        [] { (void)parse_scene_endpoint("world.example:0"); },
        "port zero must fail");
    require_throws(
        [] { (void)parse_scene_endpoint("2001:db8::1:19100"); },
        "unbracketed IPv6 must fail");
}

void test_live_startup_sequence_over_fragmented_stream() {
    FakeByteStream stream;
    FrameChannel channel(stream);
    SceneSession session(channel);

    const auto startup = session.start("region-1", "signed-ticket");

    require(startup.hello.server == "opengenesis-scene", "HELLO startup mismatch");
    require(startup.join.region_id == "region-1", "JOIN startup mismatch");
    require(startup.join.avatar_id == 77U, "JOIN avatar mismatch");
    require(payload_as_string(startup.initial_sync).starts_with("mode=snapshot\n"),
            "initial sync must carry snapshot response");

    require(stream.sent.size() == 3U, "startup must send exactly three requests");
    require(stream.sent[0].type == MessageType::hello, "startup request 1 must be HELLO");
    require(stream.sent[1].type == MessageType::scene_join,
            "startup request 2 must be SCENE_JOIN");
    require(stream.sent[2].type == MessageType::scene_sync_request,
            "startup request 3 must be SCENE_SYNC_REQUEST");
    require(payload_as_string(stream.sent[1]) ==
            "region=region-1\nticket=signed-ticket\n",
            "live SCENE_JOIN payload mismatch");
    require(payload_as_string(stream.sent[2]) ==
            "since=0\nmax_events=256\n",
            "live initial sync payload mismatch");

    require(stream.sent[0].request_id != stream.sent[1].request_id &&
            stream.sent[1].request_id != stream.sent[2].request_id,
            "startup request ids must be unique");

    const auto deferred = session.receive_next();
    require(deferred.type == MessageType::chat_event,
            "unrelated Scene frame was not deferred");
    require(payload_as_string(deferred) == "message=deferred\n",
            "deferred Scene frame payload mismatch");

    session.disconnect();
    require(!stream.open, "disconnect must close byte stream");
    require(stream.sent.back().type == MessageType::goodbye,
            "disconnect must send GOODBYE");
}

void test_start_can_resume_from_authoritative_sequence() {
    FakeByteStream stream;
    FrameChannel channel(stream);
    SceneSession session(channel);

    (void)session.start(
        "region-1",
        "signed-ticket",
        55U);

    require(stream.sent.size() == 3U,
            "resumed startup must send exactly three requests");
    require(
        payload_as_string(stream.sent[2]) ==
            "since=55\nmax_events=256\n",
        "resumed startup must request delta from the supplied sequence");
}

void test_avatar_reconcile_over_live_session() {
    FakeByteStream stream;
    FrameChannel channel(stream);
    SceneSession session(channel);

    (void)session.start(
        "region-1",
        "signed-ticket");

    const auto ack = session.reconcile_avatar({
        .client_sequence = 5U,
        .pose = {
            .x = 129.0,
            .y = 130.0,
            .z = 25.0,
            .rx = 0.0,
            .ry = 0.0,
            .rz = 90.0,
        },
        .velocity = {
            .x = 0.0,
            .y = 4.0,
            .z = 0.0,
        },
    });

    require(ack.client_sequence == 5U,
            "live reconcile client sequence mismatch");
    require(ack.server_sequence == 130U,
            "live reconcile server sequence mismatch");
    require(ack.pose.y == 130.0,
            "live reconcile pose mismatch");

    require(stream.sent.back().type ==
                MessageType::avatar_reconcile,
            "Scene session did not send AVATAR_RECONCILE");
    require(
        payload_as_string(stream.sent.back()).starts_with(
            "client_sequence=5\n"),
        "Scene session reconcile payload mismatch");
}

void test_start_requires_open_channel() {
    FakeByteStream stream;
    stream.close();
    FrameChannel channel(stream);
    SceneSession session(channel);

    require_throws(
        [&] { (void)session.start("region-1", "ticket"); },
        "session start on closed channel must fail");
}

} // namespace

int main() {
    try {
        test_endpoint_parser();
        test_live_startup_sequence_over_fragmented_stream();
        test_start_can_resume_from_authoritative_sequence();
        test_avatar_reconcile_over_live_session();
        test_start_requires_open_channel();
        std::cout << "OpenGenesisLINK Viewer Scene session tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr << "Scene session test failure: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}
