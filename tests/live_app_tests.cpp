#include "opengenesislink/viewer/app/launch_options.hpp"
#include "opengenesislink/viewer/app/login_flow.hpp"
#include "opengenesislink/viewer/core/http_transport.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using ogl::viewer::core::HttpRequest;
using ogl::viewer::core::HttpResponse;

class FakeTransport final : public ogl::viewer::core::HttpTransport {
public:
    std::vector<HttpResponse> responses;
    std::vector<HttpRequest> requests;

    HttpResponse perform(const HttpRequest& request) override {
        requests.push_back(request);
        if (responses.empty()) {
            throw std::runtime_error("FakeTransport has no queued response");
        }
        auto response = std::move(responses.front());
        responses.erase(responses.begin());
        return response;
    }
};

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

void test_core_entry_order_and_contracts() {
    FakeTransport transport;
    transport.responses = {
        {200, R"({
            "version":"1.0.0-alpha.1",
            "api_version":1,
            "capabilities":["release-contract-v1","viewer-bootstrap-v1"]
        })"},
        {200, R"({
            "release_contract":"ogl-release-v1",
            "server_version":"1.0.0-alpha.1",
            "channel":"alpha",
            "api_version":1,
            "contracts":{
                "viewer":"ogl-viewer-bootstrap-v1",
                "scene":"scene-v2",
                "atlas":"ogl-atlas-v1",
                "federation":"OGL-FED/2",
                "voice":"ogl-voice-cap-v0"
            }
        })"},
        {200, R"({
            "user":{
                "id":"user-1",
                "username":"resident",
                "display_name":"Resident"
            },
            "token":"session-token",
            "expires_unix":2000000000
        })"},
        {200, R"({
            "viewer_contract":"ogl-viewer-bootstrap-v1",
            "scene_contract":"scene-v2",
            "scene_endpoint":"world.example:19100",
            "scene_ticket":"signed-ticket",
            "capabilities":[
                "scene.sync",
                "scene.avatar.reconcile",
                "scene.region.metadata"
            ],
            "region":{"id":"region-1"},
            "spawn":{"x":120,"y":121,"z":25}
        })"}
    };

    ogl::viewer::app::CoreEntryCoordinator coordinator(transport);
    const auto result = coordinator.enter({
        .core_base_url = "https://core.example",
        .username = "resident",
        .password = "secret",
        .region_id = "region-1",
        .spawn_x = 120.0,
        .spawn_y = 121.0,
        .spawn_z = 25.0,
    });

    require(transport.requests.size() == 4U,
            "Core entry must execute four HTTP requests");
    require(transport.requests[0].url == "https://core.example/v1",
            "Core entry must discover /v1 first");
    require(transport.requests[1].url == "https://core.example/v1/release",
            "Core entry must discover /v1/release second");
    require(transport.requests[2].url == "https://core.example/v1/auth/login",
            "Core entry must authenticate after discovery");
    require(transport.requests[3].url == "https://core.example/v1/viewer/bootstrap",
            "Core entry must bootstrap after authentication");

    require(result.discovery.release.server_version == "1.0.0-alpha.1",
            "Core entry server version mismatch");
    require(result.login.token == "session-token",
            "Core entry login token mismatch");
    require(result.bootstrap.scene_endpoint == "world.example:19100",
            "Core entry Scene endpoint mismatch");
    require(result.bootstrap.scene_ticket == "signed-ticket",
            "Core entry Scene ticket mismatch");

    const auto authorization =
        transport.requests[3].headers.find("Authorization");
    require(
        authorization != transport.requests[3].headers.end() &&
        authorization->second == "Bearer session-token",
        "Viewer bootstrap must use the authenticated bearer token");
}

void test_live_launch_parser() {
    const std::vector<std::string_view> arguments{
        "--connect",
        "--server", "https://core.example",
        "--username", "resident",
        "--region", "region-1",
        "--spawn", "10.5", "20.25", "30"
    };

    const auto parsed =
        ogl::viewer::app::parse_command_line(
            arguments,
            "environment-password");

    require(parsed.launch.live_login.has_value(),
            "live launch request missing");
    require(!parsed.launch.render_demo,
            "live launch must not enable render demo");
    require(parsed.launch.live_login->core_base_url ==
                "https://core.example",
            "live launch server mismatch");
    require(parsed.launch.live_login->username == "resident",
            "live launch username mismatch");
    require(parsed.launch.live_login->password ==
                "environment-password",
            "live launch did not use environment password");
    require(parsed.launch.live_login->region_id == "region-1",
            "live launch region mismatch");
    require(parsed.launch.live_login->spawn_x == 10.5 &&
            parsed.launch.live_login->spawn_y == 20.25 &&
            parsed.launch.live_login->spawn_z == 30.0,
            "live launch spawn mismatch");
}

void test_launch_parser_rejects_unsafe_or_incomplete_live_login() {
    require_throws([] {
        const std::vector<std::string_view> arguments{
            "--connect",
            "--server", "https://core.example",
            "--username", "resident",
            "--region", "region-1"
        };
        (void)ogl::viewer::app::parse_command_line(
            arguments,
            "");
    }, "live login without environment password must fail");

    require_throws([] {
        const std::vector<std::string_view> arguments{
            "--connect",
            "--render-demo",
            "--server", "https://core.example",
            "--username", "resident",
            "--region", "region-1"
        };
        (void)ogl::viewer::app::parse_command_line(
            arguments,
            "secret");
    }, "live connection and render demo must be mutually exclusive");

    require_throws([] {
        const std::vector<std::string_view> arguments{
            "--password", "secret"
        };
        (void)ogl::viewer::app::parse_command_line(
            arguments,
            "");
    }, "password command-line argument must not be accepted");
}

} // namespace

int main() {
    try {
        test_core_entry_order_and_contracts();
        test_live_launch_parser();
        test_launch_parser_rejects_unsafe_or_incomplete_live_login();
        std::cout << "OpenGenesisLINK Viewer live app tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr << "Live app test failure: "
                  << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}
