#include "opengenesislink/viewer/core/curl_http_transport.hpp"
#include "opengenesislink/viewer/core/release_discovery.hpp"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: ogl-viewer-contract-smoke <core-base-url>\n";
        return 2;
    }

    try {
        ogl::viewer::core::CurlHttpTransport transport;
        ogl::viewer::core::ReleaseDiscovery discovery(transport);
        const auto result = discovery.discover(argv[1]);

        std::cout << "Compatible OpenGenesisLINK Core detected\n"
                  << "Server: " << result.release.server_version << "\n"
                  << "Channel: " << result.release.channel << "\n"
                  << "Release contract: " << result.release.release_contract << "\n"
                  << "Viewer contract: " << result.release.viewer_contract << "\n"
                  << "Scene contract: " << result.release.scene_contract << "\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Compatibility check failed: " << ex.what() << "\n";
        return 1;
    }
}
