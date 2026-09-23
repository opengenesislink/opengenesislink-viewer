#include "opengenesislink/viewer/core/release_discovery.hpp"

#include "opengenesislink/viewer/contracts.hpp"

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace ogl::viewer::core {
namespace {

using nlohmann::json;

std::string endpoint(const std::string& base, std::string_view path) {
    if (base.empty()) {
        throw std::invalid_argument("Core base URL must not be empty");
    }
    if (base.back() == '/') {
        return base.substr(0, base.size() - 1) + std::string(path);
    }
    return base + std::string(path);
}

json parse_ok_json(const HttpResponse& response, std::string_view operation) {
    if (response.status < 200 || response.status >= 300) {
        throw std::runtime_error(
            std::string(operation) + " failed with HTTP " +
            std::to_string(response.status));
    }

    try {
        return json::parse(response.body);
    } catch (const json::exception& ex) {
        throw std::runtime_error(
            std::string(operation) + " returned invalid JSON: " + ex.what());
    }
}

std::string optional_string(const json& value, const char* key) {
    const auto it = value.find(key);
    if (it == value.end() || !it->is_string()) {
        return {};
    }
    return it->get<std::string>();
}

std::map<std::string, std::string> string_map(const json& value, const char* key) {
    std::map<std::string, std::string> result;
    const auto it = value.find(key);
    if (it == value.end() || !it->is_object()) {
        return result;
    }
    for (auto entry = it->begin(); entry != it->end(); ++entry) {
        if (entry.value().is_string()) {
            result.emplace(entry.key(), entry.value().get<std::string>());
        }
    }
    return result;
}

} // namespace

ReleaseDiscovery::ReleaseDiscovery(HttpTransport& transport)
    : transport_(transport) {}

DiscoveryResult ReleaseDiscovery::discover(const std::string& core_base_url) const {
    const auto root_response = transport_.perform({
        .method = "GET",
        .url = endpoint(core_base_url, "/v1"),
    });
    const auto root_json = parse_ok_json(root_response, "GET /v1");

    CoreRootInfo root;
    root.api_version = root_json.value("api_version", 0);
    root.server_version = optional_string(root_json, "version");
    if (root.server_version.empty()) {
        root.server_version = optional_string(root_json, "server_version");
    }
    if (const auto it = root_json.find("capabilities");
        it != root_json.end() && it->is_array()) {
        for (const auto& capability : *it) {
            if (capability.is_string()) {
                root.capabilities.push_back(capability.get<std::string>());
            }
        }
    }
    root.endpoints = string_map(root_json, "endpoints");

    if (root.api_version != contracts::api_version) {
        throw std::runtime_error("Unsupported Core API version");
    }

    const auto release_response = transport_.perform({
        .method = "GET",
        .url = endpoint(core_base_url, "/v1/release"),
    });
    const auto release_json = parse_ok_json(release_response, "GET /v1/release");

    ReleaseInfo release;
    release.release_contract = optional_string(release_json, "release_contract");
    release.server_version = optional_string(release_json, "server_version");
    release.channel = optional_string(release_json, "channel");
    release.api_version = release_json.value("api_version", 0);
    release.endpoints = string_map(release_json, "endpoints");

    const auto contracts_json = release_json.find("contracts");
    if (contracts_json == release_json.end() || !contracts_json->is_object()) {
        throw std::runtime_error("Release discovery is missing contracts");
    }

    release.viewer_contract = optional_string(*contracts_json, "viewer");
    release.scene_contract = optional_string(*contracts_json, "scene");
    release.atlas_contract = optional_string(*contracts_json, "atlas");
    release.federation_contract = optional_string(*contracts_json, "federation");
    release.voice_contract = optional_string(*contracts_json, "voice");

    if (release.release_contract != contracts::release) {
        throw std::runtime_error("Unsupported release contract: " +
                                 release.release_contract);
    }
    if (release.api_version != contracts::api_version) {
        throw std::runtime_error("Unsupported release API version");
    }
    if (release.viewer_contract != contracts::viewer_bootstrap) {
        throw std::runtime_error("Unsupported Viewer bootstrap contract: " +
                                 release.viewer_contract);
    }
    if (release.scene_contract != contracts::scene) {
        throw std::runtime_error("Unsupported Scene contract: " +
                                 release.scene_contract);
    }

    return {
        .root = std::move(root),
        .release = std::move(release),
    };
}

} // namespace ogl::viewer::core
