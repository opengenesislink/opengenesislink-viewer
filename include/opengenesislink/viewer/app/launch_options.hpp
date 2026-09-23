#pragma once

#include "opengenesislink/viewer/app/desktop_application.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ogl::viewer::app {

struct CommandLineOptions {
    bool show_help = false;
    bool show_version = false;
    DesktopLaunchOptions launch;
};

[[nodiscard]] CommandLineOptions parse_command_line(
    std::span<const std::string_view> arguments,
    std::string_view password_from_environment);

[[nodiscard]] std::string command_line_help();

} // namespace ogl::viewer::app
