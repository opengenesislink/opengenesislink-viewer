#pragma once

#include "opengenesislink/viewer/core/http_transport.hpp"

#include <optional>
#include <string>
#include <vector>

namespace ogl::viewer::core {

struct SpawnPoint {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct ViewerBootstrapRequest {
    std::string region;
    double x = 128.0;
    double y = 128.0;
    double z = 25.0;
};

struct ViewerBootstrapResult {
    std::string viewer_contract;
    std::string scene_contract;
    std::string scene_endpoint;
    std::string scene_ticket;
    std::string region_id;
    std::vector<std::string> capabilities;
    std::optional<SpawnPoint> spawn;
    std::string raw_json;
};

class ViewerBootstrapClient {
public:
    explicit ViewerBootstrapClient(HttpTransport& transport);

    ViewerBootstrapResult bootstrap(
        const std::string& core_base_url,
        const std::string& bearer_token,
        const ViewerBootstrapRequest& request) const;

private:
    HttpTransport& transport_;
};

} // namespace ogl::viewer::core
