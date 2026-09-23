#include "opengenesislink/viewer/app/desktop_application.hpp"
#include "opengenesislink/viewer/app/launch_options.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string_view>
#include <vector>

int main(int argc, char** argv) {
    try {
        std::vector<std::string_view> arguments;
        arguments.reserve(
            argc > 1 ? static_cast<std::size_t>(argc - 1) : 0U);
        for (int index = 1; index < argc; ++index) {
            arguments.emplace_back(argv[index]);
        }

        const char* password_environment =
            std::getenv("OGL_VIEWER_PASSWORD");
        const std::string_view password =
            password_environment != nullptr
                ? std::string_view{password_environment}
                : std::string_view{};

        const auto options =
            ogl::viewer::app::parse_command_line(
                arguments,
                password);

        if (options.show_version) {
            std::cout
                << "OpenGenesisLINK Viewer "
                << OGL_VIEWER_VERSION
                << "\n";
            return 0;
        }

        if (options.show_help) {
            std::cout
                << ogl::viewer::app::command_line_help();
            return 0;
        }

        ogl::viewer::app::DesktopApplication application;
        return application.run(options.launch);
    } catch (const std::exception& ex) {
        std::cerr
            << "OpenGenesisLINK Viewer startup failed: "
            << ex.what()
            << "\n";
        return 1;
    }
}
