#include "opengenesislink/viewer/app/desktop_application.hpp"

#include <exception>
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    if (argc > 1 && std::string_view{argv[1]} == "--version") {
        std::cout << "OpenGenesisLINK Viewer " OGL_VIEWER_VERSION << "\n";
        return 0;
    }

    const bool render_demo =
        argc > 1 && std::string_view{argv[1]} == "--render-demo";

    try {
        ogl::viewer::app::DesktopApplication application;
        return application.run(render_demo);
    } catch (const std::exception& ex) {
        std::cerr << "OpenGenesisLINK Viewer startup failed: "
                  << ex.what() << "\n";
        return 1;
    }
}
