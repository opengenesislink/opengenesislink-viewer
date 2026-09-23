#pragma once

#include "opengenesislink/viewer/core/auth_client.hpp"
#include "opengenesislink/viewer/core/http_transport.hpp"
#include "opengenesislink/viewer/core/release_discovery.hpp"
#include "opengenesislink/viewer/core/viewer_bootstrap.hpp"

#include <string>

namespace ogl::viewer::app {

struct LoginRequest {
    std::string core_base_url;
    std::string username;
    std::string password;
    std::string region_id;
    double spawn_x = 128.0;
    double spawn_y = 128.0;
    double spawn_z = 25.0;
};

struct CoreEntryResult {
    core::DiscoveryResult discovery;
    core::LoginResult login;
    core::ViewerBootstrapResult bootstrap;
};

class CoreEntryCoordinator {
public:
    explicit CoreEntryCoordinator(core::HttpTransport& transport);

    [[nodiscard]] CoreEntryResult enter(
        const LoginRequest& request) const;

private:
    core::HttpTransport& transport_;
};

} // namespace ogl::viewer::app
