#include "opengenesislink/viewer/core/viewer_bootstrap.hpp"

#include "opengenesislink/viewer/contracts.hpp"

#include <nlohmann/json.hpp>

#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ogl::viewer::core {
namespace {

std::string endpoint(const std::string& base, std::string_view path) {
    if (base.empty()) {
        throw std::invalid_argument("Core base URL must not be empty");
    }
    return (base.back() == '/')
        ? base.substr(0, base.size() - 1) + std::string(path)
        : base + std::string(path);
}

std::vector<std::string> parse_capabilities(const nlohmann::json& doc) {
    std::vector<std::string> result;
    const auto it = doc.find("capabilities");
    if (it == doc.end()) {
        return result;
    }

    if (it->is_array()) {
        for (const auto& value : *it) {
            if (value.is_string()) {
                result.push_back(value.get<std::string>());
            }
        }
        return result;
    }

    if (it->is_string()) {
        std::stringstream stream(it->get<std::string>());
        std::string item;
        while (std::getline(stream, item, ',')) {
            if (!item.empty()) {
                result.push_back(item);
            }
        }
    }

    return result;
}

} // namespace

ViewerBootstrapClient::ViewerBootstrapClient(HttpTransport& transport)
    : transport_(transport) {}

ViewerBootstrapResult ViewerBootstrapClient::bootstrap(
    const std::string& core_base_url,
    const std::string& bearer_token,
    const ViewerBootstrapRequest& request) const {

    if (bearer_token.empty()) {
        throw std::invalid_argument("Bearer token must not be empty");
    }
    if (request.region.empty()) {
        throw std::invalid_argument("Region id must not be empty");
    }

    const nlohmann::json payload = {
        {"region", request.region},
        {"x", request.x},
        {"y", request.y},
        {"z", request.z},
    };

    const auto response = transport_.perform({
        .method = "POST",
        .url = endpoint(core_base_url, "/v1/viewer/bootstrap"),
        .headers = {
            {"Authorization", "Bearer " + bearer_token},
            {"Content-Type", "application/json"},
        },
        .body = payload.dump(),
    });

    if (response.status < 200 || response.status >= 300) {
        throw std::runtime_error(
            "Viewer bootstrap failed with HTTP " + std::to_string(response.status));
    }

    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(response.body);
    } catch (const nlohmann::json::exception& ex) {
        throw std::runtime_error(
            std::string("Viewer bootstrap returned invalid JSON: ") + ex.what());
    }

    ViewerBootstrapResult result;
    result.raw_json = response.body;
    result.viewer_contract = doc.value("viewer_contract", "");
    result.scene_contract = doc.value("scene_contract", "");
    result.scene_endpoint = doc.value("scene_endpoint", "");
    result.scene_ticket = doc.value("scene_ticket", "");
    result.capabilities = parse_capabilities(doc);

    if (const auto region = doc.find("region"); region != doc.end()) {
        if (region->is_object()) {
            result.region_id = region->value("id", "");
        } else if (region->is_string()) {
            result.region_id = region->get<std::string>();
        }
    }
    if (result.region_id.empty()) {
        result.region_id = doc.value("region_id", "");
    }

    if (const auto spawn = doc.find("spawn");
        spawn != doc.end() && spawn->is_object()) {
        result.spawn = SpawnPoint{
            .x = spawn->value("x", 0.0),
            .y = spawn->value("y", 0.0),
            .z = spawn->value("z", 0.0),
        };
    }

    if (result.viewer_contract != contracts::viewer_bootstrap) {
        throw std::runtime_error("Unsupported Viewer bootstrap contract");
    }
    if (result.scene_contract != contracts::scene) {
        throw std::runtime_error("Unsupported Scene contract");
    }
    if (result.scene_endpoint.empty() || result.scene_ticket.empty()) {
        throw std::runtime_error(
            "Viewer bootstrap is missing Scene endpoint or Scene ticket");
    }

    return result;
}

} // namespace ogl::viewer::core
