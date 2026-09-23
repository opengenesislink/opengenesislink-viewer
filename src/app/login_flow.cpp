#include "opengenesislink/viewer/app/login_flow.hpp"

#include <stdexcept>
#include <utility>

namespace ogl::viewer::app {

CoreEntryCoordinator::CoreEntryCoordinator(core::HttpTransport& transport)
    : transport_(transport) {}

CoreEntryResult CoreEntryCoordinator::enter(
    const LoginRequest& request) const {
    if (request.core_base_url.empty()) {
        throw std::invalid_argument("Core base URL must not be empty");
    }
    if (request.username.empty()) {
        throw std::invalid_argument("Username must not be empty");
    }
    if (request.password.empty()) {
        throw std::invalid_argument("Password must not be empty");
    }
    if (request.region_id.empty()) {
        throw std::invalid_argument("Region id must not be empty");
    }

    core::ReleaseDiscovery discovery_client(transport_);
    auto discovery =
        discovery_client.discover(request.core_base_url);

    core::AuthClient auth_client(transport_);
    auto login = auth_client.login(
        request.core_base_url,
        request.username,
        request.password);

    core::ViewerBootstrapClient bootstrap_client(transport_);
    auto bootstrap = bootstrap_client.bootstrap(
        request.core_base_url,
        login.token,
        {
            .region = request.region_id,
            .x = request.spawn_x,
            .y = request.spawn_y,
            .z = request.spawn_z,
        });

    return {
        .discovery = std::move(discovery),
        .login = std::move(login),
        .bootstrap = std::move(bootstrap),
    };
}

} // namespace ogl::viewer::app
