#include "opengenesislink/viewer/app/desktop_application.hpp"

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

} // namespace

int DesktopApplication::run(bool render_demo) {
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

    const std::string title =
        std::string{"OpenGenesisLINK Viewer "} +
        OGL_VIEWER_VERSION +
        (render_demo ? " - Render Demo" : " - Not connected");

    GLFWwindow* window = glfwCreateWindow(
        1280,
        720,
        title.c_str(),
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

        double previous_time = glfwGetTime();
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

            renderer.render(
                render_demo ? &demo_region : nullptr,
                camera);

            glfwSwapBuffers(window);
        }
    }

    return 0;
}

} // namespace ogl::viewer::app
