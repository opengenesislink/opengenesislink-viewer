#include "opengenesislink/viewer/core/auth_client.hpp"
#include "opengenesislink/viewer/core/http_transport.hpp"
#include "opengenesislink/viewer/core/release_discovery.hpp"
#include "opengenesislink/viewer/core/viewer_bootstrap.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
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

void test_release_discovery() {
    FakeTransport transport;
    transport.responses = {
        {200, R"({
            "version":"1.0.0-alpha.1",
            "api_version":1,
            "capabilities":["release-contract-v1","viewer-bootstrap-v1"],
            "endpoints":{"release":"GET /v1/release"},
            "future_field":{"ignored":true}
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
            },
            "unknown_additive_field":42
        })"}
    };

    ogl::viewer::core::ReleaseDiscovery discovery(transport);
    const auto result = discovery.discover("https://core.example");

    require(transport.requests.size() == 2, "discovery must make two requests");
    require(transport.requests[0].url == "https://core.example/v1", "wrong /v1 URL");
    require(transport.requests[1].url == "https://core.example/v1/release",
            "wrong release URL");
    require(result.release.viewer_contract == "ogl-viewer-bootstrap-v1",
            "wrong viewer contract");
    require(result.release.scene_contract == "scene-v2", "wrong scene contract");
}

void test_release_rejects_breaking_contract() {
    FakeTransport transport;
    transport.responses = {
        {200, R"({"api_version":1})"},
        {200, R"({
            "release_contract":"ogl-release-v2",
            "server_version":"2.0.0",
            "channel":"alpha",
            "api_version":1,
            "contracts":{"viewer":"ogl-viewer-bootstrap-v1","scene":"scene-v2"}
        })"}
    };

    ogl::viewer::core::ReleaseDiscovery discovery(transport);
    bool threw = false;
    try {
        (void)discovery.discover("https://core.example/");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    require(threw, "breaking release contract must be rejected");
}

void test_login() {
    FakeTransport transport;
    transport.responses = {
        {200, R"({
            "user":{"id":"u-1","username":"avatar","display_name":"Avatar Resident"},
            "token":"secret-session-token",
            "expires_unix":2000000000
        })"}
    };

    ogl::viewer::core::AuthClient auth(transport);
    const auto result = auth.login("https://core.example", "avatar", "password");

    require(result.token == "secret-session-token", "login token mismatch");
    require(result.user.id == "u-1", "login user mismatch");
    require(transport.requests[0].method == "POST", "login must POST");
    require(transport.requests[0].url == "https://core.example/v1/auth/login",
            "wrong login URL");
}

void test_bootstrap() {
    FakeTransport transport;
    transport.responses = {
        {200, R"({
            "viewer_contract":"ogl-viewer-bootstrap-v1",
            "scene_contract":"scene-v2",
            "scene_endpoint":"world.example:9400",
            "scene_ticket":"signed-ticket",
            "capabilities":"scene.sync,scene.avatar.reconcile,scene.region.metadata",
            "region":{"id":"region-1","name":"Start"},
            "spawn":{"x":128,"y":128,"z":25},
            "inventory":{"future_typed_model":"preserved-in-raw-json"}
        })"}
    };

    ogl::viewer::core::ViewerBootstrapClient client(transport);
    const auto result = client.bootstrap(
        "https://core.example",
        "session-token",
        {.region = "region-1", .x = 128, .y = 128, .z = 25});

    require(result.scene_endpoint == "world.example:9400", "scene endpoint mismatch");
    require(result.scene_ticket == "signed-ticket", "scene ticket mismatch");
    require(result.region_id == "region-1", "region id mismatch");
    require(result.spawn.has_value(), "spawn missing");
    require(result.capabilities.size() == 3, "capability CSV not parsed");
    require(result.raw_json.find("future_typed_model") != std::string::npos,
            "additive bootstrap JSON must be preserved");

    const auto auth = transport.requests[0].headers.find("Authorization");
    require(auth != transport.requests[0].headers.end(), "authorization header missing");
    require(auth->second == "Bearer session-token", "authorization header mismatch");
}

} // namespace

int main() {
    try {
        test_release_discovery();
        test_release_rejects_breaking_contract();
        test_login();
        test_bootstrap();
        std::cout << "OpenGenesisLINK Viewer core contract tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr << "Test failure: " << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}
