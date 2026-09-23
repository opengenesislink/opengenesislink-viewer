#pragma once

#include "opengenesislink/viewer/core/http_transport.hpp"

#include <map>
#include <string>
#include <vector>

namespace ogl::viewer::core {

struct CoreRootInfo {
    int api_version = 0;
    std::string server_version;
    std::vector<std::string> capabilities;
    std::map<std::string, std::string> endpoints;
};

struct ReleaseInfo {
    std::string release_contract;
    std::string server_version;
    std::string channel;
    int api_version = 0;
    std::string viewer_contract;
    std::string scene_contract;
    std::string atlas_contract;
    std::string federation_contract;
    std::string voice_contract;
    std::map<std::string, std::string> endpoints;
};

struct DiscoveryResult {
    CoreRootInfo root;
    ReleaseInfo release;
};

class ReleaseDiscovery {
public:
    explicit ReleaseDiscovery(HttpTransport& transport);
    DiscoveryResult discover(const std::string& core_base_url) const;

private:
    HttpTransport& transport_;
};

} // namespace ogl::viewer::core
