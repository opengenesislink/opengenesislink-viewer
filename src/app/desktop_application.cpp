#include "opengenesislink/viewer/app/desktop_application.hpp"

#include "opengenesislink/viewer/app/viewer_runtime.hpp"
#include "opengenesislink/viewer/input/camera.hpp"
#include "opengenesislink/viewer/render/opengl_renderer.hpp"
#include "opengenesislink/viewer/world/render_world.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

namespace ogl::viewer::app {
namespace {

void glfw_error_callback(int code, const char* description) {
    std::cerr << "GLFW error " << code << ": "
              << (description != nullptr ? description : "unknown")
              << "\n";
}

world::RenderRegion make_demo_region() {
    world::RenderRegion region;
    region.region_id = "viewer-render-demo";
    region.scene_sequence = 0U;
    region.terrain_revision = 1U;
    region.terrain_width = 256U;
    region.terrain_height = 256U;
    region.terrain_cell_size = 1.0;
    region.water_height = 20.0;

    world::RenderInstance object;
    object.entity_id = 1U;
    object.geometry = world::RenderGeometry::box_proxy;
    object.transform.position = {128.0, 128.0, 22.0};
    object.transform.scale = {4.0, 4.0, 4.0};
    region.instances.emplace(object.entity_id, object);

    world::RenderInstance avatar;
    avatar.entity_id = 2U;
    avatar.geometry = world::RenderGeometry::avatar_capsule;
    avatar.transform.position = {134.0, 128.0, 21.0};
    avatar.transform.scale = {0.7, 0.7, 1.8};
    region.instances.emplace(avatar.entity_id, avatar);

    return region;
}

input::CameraInput read_camera_input(
    GLFWwindow* window,
    double delta_seconds) {
    input::CameraInput input;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        input.forward += 1.0;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        input.forward -= 1.0;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        input.right += 1.0;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        input.right -= 1.0;
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        input.up += 1.0;
    }
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        input.up -= 1.0;
    }

    constexpr double look_speed = 90.0;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        input.yaw_delta += look_speed * delta_seconds;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        input.yaw_delta -= look_speed * delta_seconds;
    }
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        input.pitch_delta += look_speed * delta_seconds;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        input.pitch_delta -= look_speed * delta_seconds;
    }

    return input;
}

std::string connected_title(const ConnectionInfo& info) {
    return std::string{"OpenGenesisLINK Viewer "} +
           OGL_VIEWER_VERSION +
           " - " + info.region_id +
           " - " + info.username;
}

void position_camera_at_spawn(
    input::CameraState& camera,
    const ConnectionInfo& info) {
    if (!info.spawn.has_value()) {
        return;
    }

    camera.position = {
        info.spawn->x,
        std::max(0.0, info.spawn->y - 12.0),
        info.spawn->z + 3.0,
    };
    camera.yaw_degrees = 90.0;
    camera.pitch_degrees = -12.0;
}

} // namespace

int DesktopApplication::run(
    const DesktopLaunchOptions& options) {
    glfwSetErrorCallback(&glfw_error_callback);

    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("GLFW initialization failed");
    }

    struct GlfwGuard {
        ~GlfwGuard() {
            glfwTerminate();
        }
    } glfw_guard;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    const std::string initial_title =
        std::string{"OpenGenesisLINK Viewer "} +
        OGL_VIEWER_VERSION +
        (options.render_demo
             ? " - Render Demo"
             : options.live_login.has_value()
                   ? " - Connecting"
                   : " - Not connected");

    GLFWwindow* window = glfwCreateWindow(
        1280,
        720,
        initial_title.c_str(),
        nullptr,
        nullptr);
    if (window == nullptr) {
        throw std::runtime_error("Unable to create Viewer window");
    }

    struct WindowGuard {
        GLFWwindow* value = nullptr;
        ~WindowGuard() {
            if (value != nullptr) {
                glfwDestroyWindow(value);
            }
        }
    } window_guard{window};

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    {
        render::OpenGlRenderer renderer;
        renderer.initialize();

        input::CameraState camera;
        camera.position = {128.0, 116.0, 28.0};
        camera.yaw_degrees = 90.0;
        camera.pitch_degrees = -12.0;
        camera.move_speed = 12.0;
        input::CameraController camera_controller;

        auto demo_region = make_demo_region();
        ViewerRuntime live_runtime;

        if (options.live_login.has_value()) {
            glfwSetWindowTitle(
                window,
                "OpenGenesisLINK Viewer - Connecting...");
            const auto info =
                live_runtime.connect(*options.live_login);
            position_camera_at_spawn(camera, info);
            const auto title = connected_title(info);
            glfwSetWindowTitle(window, title.c_str());
            std::cout
                << "Connected to OpenGenesisLINK "
                << info.server_version
                << " region " << info.region_id
                << " as " << info.username
                << "\n";
        }

        double previous_time = glfwGetTime();
        double next_scene_poll = previous_time + 0.5;
        double next_reconnect_attempt = 0.0;
        bool reconnect_pending = false;

        int previous_width = 0;
        int previous_height = 0;

        while (glfwWindowShouldClose(window) == GLFW_FALSE) {
            glfwPollEvents();

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            const auto current_time = glfwGetTime();
            const auto delta_seconds =
                std::clamp(current_time - previous_time, 0.0, 0.1);
            previous_time = current_time;

            if (options.live_login.has_value()) {
                if (live_runtime.connected() &&
                    current_time >= next_scene_poll) {
                    try {
                        const auto synchronized =
                            live_runtime.poll(256U);
                        (void)synchronized;
                        next_scene_poll = current_time + 0.5;
                    } catch (const std::exception& ex) {
                        std::cerr
                            << "Scene synchronization failed: "
                            << ex.what() << "\n";
                        reconnect_pending = true;
                        next_reconnect_attempt = current_time + 1.0;
                        glfwSetWindowTitle(
                            window,
                            "OpenGenesisLINK Viewer - Connection lost");
                    }
                }

                if (reconnect_pending &&
                    current_time >= next_reconnect_attempt) {
                    try {
                        const auto info = live_runtime.reconnect();
                        reconnect_pending = false;
                        next_scene_poll = current_time + 0.5;
                        const auto title = connected_title(info);
                        glfwSetWindowTitle(window, title.c_str());
                        std::cout
                            << "Scene connection resumed at sequence "
                            << live_runtime.world_model().sequence()
                            << "\n";
                    } catch (const std::exception& ex) {
                        std::cerr
                            << "Scene reconnect failed: "
                            << ex.what() << "\n";
                        next_reconnect_attempt = current_time + 5.0;
                        glfwSetWindowTitle(
                            window,
                            "OpenGenesisLINK Viewer - Reconnecting...");
                    }
                }
            }

            const auto camera_input =
                read_camera_input(window, delta_seconds);
            camera_controller.update(
                camera,
                camera_input,
                delta_seconds);

            int width = 0;
            int height = 0;
            glfwGetFramebufferSize(window, &width, &height);
            if (width != previous_width || height != previous_height) {
                renderer.resize(width, height);
                previous_width = width;
                previous_height = height;
            }

            const world::RenderRegion* render_region = nullptr;
            if (options.live_login.has_value() &&
                live_runtime.world_model().initialized()) {
                render_region = &live_runtime.render_region();
            } else if (options.render_demo) {
                render_region = &demo_region;
            }

            renderer.render(render_region, camera);
            glfwSwapBuffers(window);
        }
    }

    return 0;
}

} // namespace ogl::viewer::app
