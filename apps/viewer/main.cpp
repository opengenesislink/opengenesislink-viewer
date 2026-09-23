#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    if (argc > 1 && std::string_view{argv[1]} == "--version") {
        std::cout << "OpenGenesisLINK Viewer " OGL_VIEWER_VERSION << "\n";
        return 0;
    }

    std::cout
        << "OpenGenesisLINK Viewer " OGL_VIEWER_VERSION << "\n"
        << "Viewer shell initialized. Graphical rendering/UI is not linked yet.\n";
    return 0;
}
