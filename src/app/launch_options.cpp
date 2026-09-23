#include "opengenesislink/viewer/app/launch_options.hpp"

#include <charconv>
#include <stdexcept>
#include <string>

namespace ogl::viewer::app {
namespace {

std::string_view require_value(
    std::span<const std::string_view> arguments,
    std::size_t& index,
    std::string_view option) {
    if (index + 1U >= arguments.size()) {
        throw std::invalid_argument(
            "Missing value for " + std::string(option));
    }
    ++index;
    if (arguments[index].empty()) {
        throw std::invalid_argument(
            "Empty value for " + std::string(option));
    }
    return arguments[index];
}

double parse_double(
    std::string_view value,
    std::string_view option) {
    double result = 0.0;
    const auto [ptr, error] =
        std::from_chars(
            value.data(),
            value.data() + value.size(),
            result);
    if (error != std::errc{} ||
        ptr != value.data() + value.size()) {
        throw std::invalid_argument(
            "Invalid number for " + std::string(option));
    }
    return result;
}

} // namespace

CommandLineOptions parse_command_line(
    std::span<const std::string_view> arguments,
    std::string_view password_from_environment) {
    CommandLineOptions result;

    bool connect = false;
    std::string server;
    std::string username;
    std::string region;
    double spawn_x = 128.0;
    double spawn_y = 128.0;
    double spawn_z = 25.0;

    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const auto argument = arguments[index];

        if (argument == "--help" || argument == "-h") {
            result.show_help = true;
        } else if (argument == "--version") {
            result.show_version = true;
        } else if (argument == "--render-demo") {
            result.launch.render_demo = true;
        } else if (argument == "--connect") {
            connect = true;
        } else if (argument == "--server") {
            server = require_value(arguments, index, argument);
        } else if (argument == "--username") {
            username = require_value(arguments, index, argument);
        } else if (argument == "--region") {
            region = require_value(arguments, index, argument);
        } else if (argument == "--spawn") {
            spawn_x = parse_double(
                require_value(arguments, index, argument),
                "--spawn x");
            spawn_y = parse_double(
                require_value(arguments, index, argument),
                "--spawn y");
            spawn_z = parse_double(
                require_value(arguments, index, argument),
                "--spawn z");
        } else {
            throw std::invalid_argument(
                "Unknown Viewer option: " + std::string(argument));
        }
    }

    if (result.show_help || result.show_version) {
        return result;
    }

    if (connect && result.launch.render_demo) {
        throw std::invalid_argument(
            "--connect and --render-demo cannot be combined");
    }

    if (!connect) {
        if (!server.empty() || !username.empty() || !region.empty()) {
            throw std::invalid_argument(
                "--server, --username and --region require --connect");
        }
        return result;
    }

    if (server.empty()) {
        throw std::invalid_argument(
            "--connect requires --server");
    }
    if (username.empty()) {
        throw std::invalid_argument(
            "--connect requires --username");
    }
    if (region.empty()) {
        throw std::invalid_argument(
            "--connect requires --region");
    }
    if (password_from_environment.empty()) {
        throw std::invalid_argument(
            "--connect requires OGL_VIEWER_PASSWORD");
    }

    result.launch.live_login = LoginRequest{
        .core_base_url = std::move(server),
        .username = std::move(username),
        .password = std::string(password_from_environment),
        .region_id = std::move(region),
        .spawn_x = spawn_x,
        .spawn_y = spawn_y,
        .spawn_z = spawn_z,
    };
    return result;
}

std::string command_line_help() {
    return
        "OpenGenesisLINK Viewer\n"
        "\n"
        "Usage:\n"
        "  ogl-viewer [--render-demo]\n"
        "  ogl-viewer --connect --server <core-url> --username <name> "
        "--region <region-id> [--spawn <x> <y> <z>]\n"
        "\n"
        "Live login reads the password from OGL_VIEWER_PASSWORD. "
        "No password command-line option is provided.\n"
        "\n"
        "Options:\n"
        "  --connect       Connect to a live OpenGenesisLINK server\n"
        "  --server URL    Core base URL, for example https://grid.example\n"
        "  --username NAME Login username\n"
        "  --region ID     Target Region id\n"
        "  --spawn X Y Z   Requested spawn position (default 128 128 25)\n"
        "  --render-demo   Local renderer demo without server state\n"
        "  --version       Print Viewer version\n"
        "  --help, -h      Show this help\n";
}

} // namespace ogl::viewer::app
