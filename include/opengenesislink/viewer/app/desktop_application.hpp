#pragma once

namespace ogl::viewer::app {

class DesktopApplication {
public:
    [[nodiscard]] int run(bool render_demo = false);
};

} // namespace ogl::viewer::app
