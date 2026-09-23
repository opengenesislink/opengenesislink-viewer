#pragma once

#include "opengenesislink/viewer/app/login_flow.hpp"

#include <optional>

namespace ogl::viewer::app {

struct DesktopLaunchOptions {
    bool render_demo = false;
    std::optional<LoginRequest> live_login;
};

class DesktopApplication {
public:
    [[nodiscard]] int run(
        const DesktopLaunchOptions& options = {});
};

} // namespace ogl::viewer::app
