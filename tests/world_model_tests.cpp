#include "opengenesislink/viewer/world/world_model.hpp"

#include "opengenesislink/viewer/scene/frame.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using ogl::viewer::scene::Frame;
using ogl::viewer::scene::MessageType;
using ogl::viewer::scene::payload_from_string;
using ogl::viewer::world::EntityKind;
using ogl::viewer::world::SceneSyncMode;
using ogl::viewer::world::WorldModel;
using ogl::viewer::world::parse_scene_sync;

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

Frame sync_frame(std::string payload) {
    return {
        .type = MessageType::scene_sync,
        .request_id = 99U,
        .payload = payload_from_string(payload),
    };
}

std::string snapshot_payload() {
    return
        "mode=snapshot\n"
        "region=region-1\n"
        "sequence=10\n"
        "terrain_width=256\n"
        "terrain_height=256\n"
        "terrain_cell_size=1.000\n"
        "terrain_revision=3\n"
        "water_height=20.000\n"
        "entity_count=2\n"
        "entity=100|object|Cube|10.000|20.000|30.000|0.000|0.000|90.000|1.000|2.000|3.000|owner-1|group-1|31|7|1|0|1|1|Hello|1.000|2.000|3.000|0.100|0.200|0.300|5.000|0.500|0.400|0.000\n"
        "entity=200|avatar|Avatar Resident|128.000|128.000|25.000|0.000|0.000|0.000|0.500|0.500|1.800|user-1||31|0|0|0|1|1||0|0|0|0|0|0|80.000|0.000|0.500|0.000\n"
        "future_additive_field=ignored\n";
}

void test_snapshot_parse_and_apply() {
    const auto sync = parse_scene_sync(sync_frame(snapshot_payload()));
    require(sync.mode == SceneSyncMode::snapshot, "snapshot mode mismatch");
    require(sync.snapshot.has_value(), "snapshot payload missing");
    require(sync.snapshot->region_id == "region-1", "snapshot region mismatch");
    require(sync.snapshot->sequence == 10U, "snapshot sequence mismatch");
    require(sync.snapshot->terrain_width == 256U, "terrain width mismatch");
    require(sync.snapshot->terrain_revision == 3U, "terrain revision mismatch");
    require(sync.snapshot->entities.size() == 2U, "snapshot entity count mismatch");

    const auto object = sync.snapshot->entities.find(100U);
    require(object != sync.snapshot->entities.end(), "object entity missing");
    require(object->second.kind == EntityKind::object, "object kind mismatch");
    require(object->second.name == "Cube", "object name mismatch");
    require(object->second.transform.position.z == 30.0, "object position mismatch");
    require(object->second.transform.scale.y == 2.0, "object scale mismatch");
    require(object->second.owner_user_id == "owner-1", "object owner mismatch");
    require(object->second.owner_permissions == 31U, "object permissions mismatch");
    require(object->second.physics.physical, "object physical flag mismatch");
    require(object->second.physics.velocity.y == 2.0, "object velocity mismatch");
    require(object->second.physics.mass == 5.0, "object mass mismatch");

    const auto avatar = sync.snapshot->entities.find(200U);
    require(avatar != sync.snapshot->entities.end(), "avatar entity missing");
    require(avatar->second.kind == EntityKind::avatar, "avatar kind mismatch");

    WorldModel model;
    const auto applied = model.apply(sync);
    require(applied.applied, "snapshot was not applied");
    require(!applied.requires_snapshot, "snapshot requested another snapshot");
    require(model.initialized(), "WorldModel was not initialized");
    require(model.sequence() == 10U, "WorldModel sequence mismatch");
}

void test_safe_delta_application() {
    WorldModel model;
    (void)model.apply(parse_scene_sync(sync_frame(snapshot_payload())));

    const auto delta = parse_scene_sync(sync_frame(
        "mode=delta\n"
        "region=region-1\n"
        "from=10\n"
        "latest=12\n"
        "count=2\n"
        "event=11|entity_updated|100||11.000|22.000|33.000|0.000|0.000|45.000|1.000|2.000|3.000\n"
        "event=12|object_text_updated|100|Updated text|11.000|22.000|33.000|0.000|0.000|45.000|1.000|2.000|3.000\n"));

    require(delta.mode == SceneSyncMode::delta, "delta mode mismatch");
    const auto applied = model.apply(delta);
    require(applied.applied, "safe delta was not applied");
    require(!applied.requires_snapshot, "safe delta unexpectedly requested snapshot");
    require(applied.events_applied == 2U, "delta applied count mismatch");
    require(model.sequence() == 12U, "delta sequence mismatch");

    const auto& entity = model.region().entities.at(100U);
    require(entity.transform.position.x == 11.0, "delta transform not applied");
    require(entity.floating_text == "Updated text", "floating text delta not applied");
}

void test_structural_delta_requests_snapshot() {
    WorldModel model;
    (void)model.apply(parse_scene_sync(sync_frame(snapshot_payload())));

    const auto delta = parse_scene_sync(sync_frame(
        "mode=delta\n"
        "region=region-1\n"
        "from=10\n"
        "latest=11\n"
        "count=1\n"
        "event=11|entity_created|300|New Object|1.000|2.000|3.000|0.000|0.000|0.000|1.000|1.000|1.000\n"));

    const auto applied = model.apply(delta);
    require(applied.applied, "structural delta sequence should still be consumed");
    require(applied.requires_snapshot,
            "entity_created must request authoritative full entity snapshot");
    require(model.region().entities.find(300U) == model.region().entities.end(),
            "WorldModel must not invent incomplete entity metadata");
}

void test_sequence_gap_requests_snapshot() {
    WorldModel model;
    (void)model.apply(parse_scene_sync(sync_frame(snapshot_payload())));

    const auto delta = parse_scene_sync(sync_frame(
        "mode=delta\n"
        "region=region-1\n"
        "from=10\n"
        "latest=13\n"
        "count=2\n"
        "event=11|entity_updated|100||999.000|2.000|3.000|0.000|0.000|0.000|1.000|1.000|1.000\n"
        "event=13|entity_updated|100||1.000|2.000|3.000|0.000|0.000|0.000|1.000|1.000|1.000\n"));

    const auto applied = model.apply(delta);
    require(!applied.applied, "gapped delta must not be accepted");
    require(applied.requires_snapshot, "gapped delta must request snapshot recovery");
    require(model.sequence() == 10U, "gapped delta changed authoritative sequence");
    require(model.region().entities.at(100U).transform.position.x == 10.0,
            "gapped delta partially mutated WorldModel");
}

void test_authoritative_avatar_reconcile_does_not_advance_sequence() {
    WorldModel model;
    (void)model.apply(
        parse_scene_sync(
            sync_frame(snapshot_payload())));

    ogl::viewer::world::Transform transform;
    transform.position = {130.0, 131.0, 25.0};
    transform.rotation = {0.0, 0.0, 45.0};
    transform.scale = {0.5, 0.5, 1.8};

    const auto applied =
        model.apply_reconciled_avatar(
            200U,
            transform,
            {1.0, 2.0, 0.0});

    require(applied,
            "authoritative avatar reconcile was not applied");
    require(model.sequence() == 10U,
            "avatar reconcile must not advance Scene sequence");

    const auto& avatar =
        model.region().entities.at(200U);
    require(avatar.transform.position.x == 130.0 &&
            avatar.transform.position.y == 131.0,
            "avatar reconcile transform mismatch");
    require(avatar.physics.velocity.x == 1.0 &&
            avatar.physics.velocity.y == 2.0,
            "avatar reconcile velocity mismatch");

    require(
        !model.apply_reconciled_avatar(
            100U,
            transform,
            {0.0, 0.0, 0.0}),
        "object entity must reject avatar reconcile");
}

void test_parser_rejects_bad_counts_and_types() {
    require_throws([] {
        (void)parse_scene_sync(sync_frame(
            "mode=snapshot\n"
            "region=r\n"
            "sequence=1\n"
            "terrain_width=256\n"
            "terrain_height=256\n"
            "terrain_cell_size=1\n"
            "terrain_revision=1\n"
            "water_height=20\n"
            "entity_count=1\n"));
    }, "snapshot entity_count mismatch must fail");

    require_throws([] {
        (void)parse_scene_sync({
            .type = MessageType::scene_snapshot,
            .request_id = 1U,
            .payload = payload_from_string("mode=snapshot\n"),
        });
    }, "non-SCENE_SYNC frame must fail");
}

} // namespace

int main() {
    try {
        test_snapshot_parse_and_apply();
        test_safe_delta_application();
        test_structural_delta_requests_snapshot();
        test_sequence_gap_requests_snapshot();
        test_authoritative_avatar_reconcile_does_not_advance_sequence();
        test_parser_rejects_bad_counts_and_types();
        std::cout << "OpenGenesisLINK Viewer WorldModel tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr << "WorldModel test failure: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}
