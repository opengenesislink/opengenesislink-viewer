#include "opengenesislink/viewer/input/camera.hpp"
#include "opengenesislink/viewer/scene/scene_session.hpp"
#include "opengenesislink/viewer/scene/terrain_protocol.hpp"
#include "opengenesislink/viewer/world/render_world.hpp"
#include "opengenesislink/viewer/world/scene_synchronizer.hpp"
#include "opengenesislink/viewer/world/terrain_cache.hpp"
#include "opengenesislink/viewer/world/world_model.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace ogl::viewer;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

scene::Frame sync_frame(std::string payload) {
    return {
        .type = scene::MessageType::scene_sync,
        .request_id = 1U,
        .payload = scene::payload_from_string(payload),
    };
}

std::string snapshot_payload(std::uint64_t sequence = 10U) {
    return
        "mode=snapshot\n"
        "region=region-1\n"
        "sequence=" + std::to_string(sequence) + "\n"
        "terrain_width=256\n"
        "terrain_height=256\n"
        "terrain_cell_size=1.000\n"
        "terrain_revision=3\n"
        "water_height=20.000\n"
        "entity_count=2\n"
        "entity=100|object|Cube|10.000|20.000|30.000|0.000|0.000|90.000|1.000|2.000|3.000|owner-1|group-1|31|7|1|0|1|1|Hello|0|0|0|0|0|0|5.000|0.500|0.400|0.000\n"
        "entity=200|avatar|Avatar|128.000|128.000|25.000|0.000|0.000|0.000|0.500|0.500|1.800|user-1||31|0|0|0|1|1||0|0|0|0|0|0|80.000|0.000|0.500|0.000\n";
}

class FakeSceneStream final : public scene::ByteStream {
public:
    bool open = true;
    std::vector<scene::Frame> sent;
    std::size_t terrain_requests = 0;
    bool structural_delta_next = false;

    void connect(const scene::SceneEndpoint&) override {
        open = true;
    }

    void send_all(std::span<const std::byte> bytes) override {
        const auto frame = scene::decode_frame(bytes);
        sent.push_back(frame);

        switch (frame.type) {
        case scene::MessageType::hello:
            queue({
                .type = scene::MessageType::hello_ack,
                .request_id = frame.request_id,
                .payload = scene::payload_from_string(
                    "protocol=1\n"
                    "server=opengenesis-scene\n"
                    "auth=scene-ticket-v1\n"
                    "scene_contract=2\n"
                    "movement=avatar-reconcile-v1\n"
                    "sync=scene-sync-v1\n"
                    "metadata=region-metadata-v1,parcel-read-v1\n"
                    "capabilities=scene-capabilities-v2\n"),
            });
            break;

        case scene::MessageType::scene_join:
            queue({
                .type = scene::MessageType::scene_join_ack,
                .request_id = frame.request_id,
                .payload = scene::payload_from_string(
                    "status=joined\n"
                    "region=region-1\n"
                    "user_id=user-1\n"
                    "avatar_id=200\n"
                    "sequence=10\n"
                    "scene_contract=2\n"
                    "sync=scene-sync-v1\n"
                    "movement=avatar-reconcile-v1\n"
                    "terrain_revision=3\n"
                    "capabilities=scene.join,scene.sync,scene.read,scene.terrain.sample\n"
                    "groups=\n"
                    "spawn_x=128\n"
                    "spawn_y=128\n"
                    "spawn_z=25\n"),
            });
            break;

        case scene::MessageType::scene_sync_request: {
            const auto request = scene::payload_as_string(frame);
            if (structural_delta_next &&
                request.find("since=10\n") != std::string::npos) {
                structural_delta_next = false;
                queue({
                    .type = scene::MessageType::scene_sync,
                    .request_id = frame.request_id,
                    .payload = scene::payload_from_string(
                        "mode=delta\n"
                        "region=region-1\n"
                        "from=10\n"
                        "latest=11\n"
                        "count=1\n"
                        "event=11|entity_created|300|New Object|1|2|3|0|0|0|1|1|1\n"),
                });
            } else {
                queue({
                    .type = scene::MessageType::scene_sync,
                    .request_id = frame.request_id,
                    .payload = scene::payload_from_string(snapshot_payload(10U)),
                });
            }
            break;
        }

        case scene::MessageType::terrain_sample_request:
            ++terrain_requests;
            queue({
                .type = scene::MessageType::terrain_sample,
                .request_id = frame.request_id,
                .payload = scene::payload_from_string(
                    "x=4.000\n"
                    "y=5.000\n"
                    "height=23.750\n"
                    "revision=3\n"),
            });
            break;

        case scene::MessageType::goodbye:
            break;

        default:
            throw std::runtime_error("unexpected fake Scene message");
        }
    }

    std::size_t receive_some(std::span<std::byte> buffer) override {
        if (incoming.empty()) {
            throw std::runtime_error("fake Scene stream has no response");
        }

        const auto count =
            std::min<std::size_t>({buffer.size(), incoming.size(), 11U});
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

    void queue(const scene::Frame& frame) {
        const auto bytes = scene::encode_frame(frame);
        incoming.insert(incoming.end(), bytes.begin(), bytes.end());
    }
};

void test_terrain_protocol() {
    const auto request =
        scene::make_terrain_sample_request(7U, 4.0, 5.0);
    require(
        request.type == scene::MessageType::terrain_sample_request,
        "terrain request type mismatch");
    require(
        scene::payload_as_string(request) == "x=4\ny=5\n",
        "terrain request payload mismatch");

    const auto sample = scene::parse_terrain_sample({
        .type = scene::MessageType::terrain_sample,
        .request_id = 7U,
        .payload = scene::payload_from_string(
            "x=4.000\ny=5.000\nheight=23.750\nrevision=3\n"),
    }, 7U);

    require(sample.height == 23.75, "terrain height mismatch");
    require(sample.revision == 3U, "terrain revision mismatch");
}

void test_render_world_projection() {
    world::WorldModel model;
    (void)model.apply(world::parse_scene_sync(
        sync_frame(snapshot_payload())));

    world::RenderWorldBuilder builder;
    const auto render = builder.build(model);

    require(render.region_id == "region-1", "render Region mismatch");
    require(render.instances.size() == 2U, "render instance count mismatch");
    require(
        render.instances.at(100U).geometry ==
            world::RenderGeometry::box_proxy,
        "object must use explicit box proxy until visual primitive contract exists");
    require(
        render.instances.at(200U).geometry ==
            world::RenderGeometry::avatar_capsule,
        "avatar proxy geometry mismatch");
    require(
        render.instances.at(100U).transform.scale.y == 2.0,
        "render transform mismatch");
}

void test_terrain_cache_and_snapshot_recovery() {
    FakeSceneStream stream;
    scene::FrameChannel channel(stream);
    scene::SceneSession session(channel);

    const auto startup = session.start("region-1", "ticket");

    world::WorldModel model;
    world::SceneSynchronizer synchronizer(session, model);
    const auto initial = synchronizer.apply(startup.initial_sync);
    require(initial.applied, "initial sync was not applied");
    require(model.sequence() == 10U, "initial WorldModel sequence mismatch");

    world::TerrainCache terrain(session);
    const auto first = terrain.sample_grid(model.region(), 4U, 5U);
    const auto second = terrain.sample_grid(model.region(), 4U, 5U);
    require(first == 23.75 && second == 23.75, "terrain cache height mismatch");
    require(stream.terrain_requests == 1U, "terrain cache did not reuse sample");
    require(terrain.cached_samples() == 1U, "terrain cache size mismatch");

    stream.structural_delta_next = true;
    const auto recovered = synchronizer.poll();
    require(recovered.applied, "structural delta recovery failed");
    require(recovered.recovered_with_snapshot, "snapshot recovery was not triggered");
    require(
        model.region().entities.find(300U) == model.region().entities.end(),
        "incomplete entity delta leaked into authoritative model");
}

void test_camera_controller() {
    input::CameraState camera;
    camera.position = {0.0, 0.0, 0.0};
    camera.yaw_degrees = 0.0;
    camera.pitch_degrees = 0.0;
    camera.move_speed = 10.0;

    input::CameraController controller;
    controller.update(
        camera,
        {
            .forward = 1.0,
            .right = 0.0,
            .up = 0.5,
            .yaw_delta = 90.0,
            .pitch_delta = 100.0,
        },
        1.0);

    require(std::abs(camera.position.x) < 0.000001, "camera yaw movement x mismatch");
    require(std::abs(camera.position.y - 10.0) < 0.000001, "camera forward movement mismatch");
    require(std::abs(camera.position.z - 5.0) < 0.000001, "camera vertical movement mismatch");
    require(camera.yaw_degrees == 90.0, "camera yaw mismatch");
    require(camera.pitch_degrees == 89.0, "camera pitch clamp mismatch");
}

} // namespace

int main() {
    try {
        test_terrain_protocol();
        test_render_world_projection();
        test_terrain_cache_and_snapshot_recovery();
        test_camera_controller();
        std::cout << "OpenGenesisLINK Viewer render-world foundation tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr << "Render-world foundation test failure: "
                  << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}
