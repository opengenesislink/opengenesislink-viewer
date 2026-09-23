#include "opengenesislink/viewer/app/desktop_application.hpp"

#include "opengenesislink/viewer/app/login_form.hpp"
#include "opengenesislink/viewer/app/viewer_runtime.hpp"
#include "opengenesislink/viewer/input/camera.hpp"
#include "opengenesislink/viewer/render/opengl_renderer.hpp"
#include "opengenesislink/viewer/render/ui_renderer.hpp"
#include "opengenesislink/viewer/world/render_world.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace ogl::viewer::app {
namespace {

struct Rect {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;

    [[nodiscard]] bool contains(
        double point_x,
        double point_y) const noexcept {
        return point_x >= static_cast<double>(x) &&
               point_x <= static_cast<double>(x + width) &&
               point_y >= static_cast<double>(y) &&
               point_y <= static_cast<double>(y + height);
    }
};

struct LoginLayout {
    Rect panel;
    Rect server;
    Rect username;
    Rect password;
    Rect region;
    Rect button;
};

struct LoginInputContext {
    LoginFormState* form = nullptr;
    bool* visible = nullptr;
    bool* submit_requested = nullptr;
};

constexpr render::UiColor kPanel{
    0.015F, 0.055F, 0.12F, 0.96F};
constexpr render::UiColor kPanelInner{
    0.025F, 0.09F, 0.18F, 0.98F};
constexpr render::UiColor kField{
    0.02F, 0.075F, 0.15F, 1.0F};
constexpr render::UiColor kBorder{
    0.16F, 0.38F, 0.68F, 1.0F};
constexpr render::UiColor kAccent{
    0.05F, 0.72F, 1.0F, 1.0F};
constexpr render::UiColor kButton{
    0.02F, 0.42F, 0.9F, 1.0F};
constexpr render::UiColor kButtonDisabled{
    0.08F, 0.18F, 0.3F, 1.0F};
constexpr render::UiColor kText{
    0.9F, 0.95F, 1.0F, 1.0F};
constexpr render::UiColor kMuted{
    0.48F, 0.66F, 0.86F, 1.0F};
constexpr render::UiColor kError{
    1.0F, 0.34F, 0.34F, 1.0F};
constexpr render::UiColor kSuccess{
    0.25F, 0.95F, 0.6F, 1.0F};

void glfw_error_callback(int code, const char* description) {
    std::cerr << "GLFW error " << code << ": "
              << (description != nullptr ? description : "unknown")
              << "\n";
}

void login_character_callback(
    GLFWwindow* window,
    unsigned int codepoint) {
    auto* context = static_cast<LoginInputContext*>(
        glfwGetWindowUserPointer(window));
    if (context == nullptr ||
        context->form == nullptr ||
        context->visible == nullptr ||
        !*context->visible ||
        context->form->connecting ||
        codepoint > 126U) {
        return;
    }

    context->form->append_ascii(
        static_cast<char>(codepoint));
}

void login_key_callback(
    GLFWwindow* window,
    int key,
    int,
    int action,
    int mods) {
    auto* context = static_cast<LoginInputContext*>(
        glfwGetWindowUserPointer(window));
    if (context == nullptr ||
        context->form == nullptr ||
        context->visible == nullptr ||
        context->submit_requested == nullptr ||
        !*context->visible ||
        context->form->connecting) {
        return;
    }

    if (action != GLFW_PRESS &&
        action != GLFW_REPEAT) {
        return;
    }

    if (key == GLFW_KEY_BACKSPACE) {
        context->form->backspace();
        return;
    }

    if (key == GLFW_KEY_TAB &&
        action == GLFW_PRESS) {
        context->form->focus_next(
            (mods & GLFW_MOD_SHIFT) != 0);
        return;
    }

    if ((key == GLFW_KEY_ENTER ||
         key == GLFW_KEY_KP_ENTER) &&
        action == GLFW_PRESS) {
        *context->submit_requested = true;
    }
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

input::CameraInput read_camera_look_input(
    GLFWwindow* window,
    double delta_seconds) {
    input::CameraInput input;
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

struct LiveAvatarInput {
    input::AvatarControlInput control;
    bool moving = false;
};

LiveAvatarInput read_avatar_control(
    GLFWwindow* window,
    const input::CameraState& camera) {
    LiveAvatarInput result;
    result.control.heading_degrees =
        camera.yaw_degrees;
    result.control.speed = 4.0;
    result.control.command_seconds = 0.1;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        result.control.forward += 1.0;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        result.control.forward -= 1.0;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        result.control.right += 1.0;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        result.control.right -= 1.0;
    }

    result.moving =
        result.control.forward != 0.0 ||
        result.control.right != 0.0;
    return result;
}

void follow_avatar_camera(
    input::CameraState& camera,
    const ViewerRuntime& runtime) {
    if (!runtime.world_model().initialized() ||
        runtime.avatar_id() == 0U) {
        return;
    }

    const auto& entities =
        runtime.world_model().region().entities;
    const auto found =
        entities.find(runtime.avatar_id());
    if (found == entities.end()) {
        return;
    }

    constexpr double pi =
        3.1415926535897932384626433832795;
    const auto heading =
        camera.yaw_degrees * pi / 180.0;
    const auto backward_x = -std::cos(heading);
    const auto backward_y = -std::sin(heading);

    camera.position = {
        found->second.transform.position.x +
            backward_x * 9.0,
        found->second.transform.position.y +
            backward_y * 9.0,
        found->second.transform.position.z + 4.5,
    };
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

LoginLayout login_layout(
    int width,
    int height) {
    const auto safe_width =
        static_cast<float>(std::max(width, 1));
    const auto safe_height =
        static_cast<float>(std::max(height, 1));

    const auto panel_width =
        std::clamp(
            safe_width - 80.0F,
            380.0F,
            620.0F);
    constexpr float panel_height = 590.0F;
    const auto panel_x =
        (safe_width - panel_width) * 0.5F;
    const auto panel_y =
        std::max(
            20.0F,
            (safe_height - panel_height) * 0.5F);

    const auto field_x = panel_x + 42.0F;
    const auto field_width = panel_width - 84.0F;
    constexpr float field_height = 48.0F;

    return {
        .panel = {
            panel_x,
            panel_y,
            panel_width,
            panel_height,
        },
        .server = {
            field_x,
            panel_y + 165.0F,
            field_width,
            field_height,
        },
        .username = {
            field_x,
            panel_y + 245.0F,
            field_width,
            field_height,
        },
        .password = {
            field_x,
            panel_y + 325.0F,
            field_width,
            field_height,
        },
        .region = {
            field_x,
            panel_y + 405.0F,
            field_width,
            field_height,
        },
        .button = {
            field_x,
            panel_y + 480.0F,
            field_width,
            54.0F,
        },
    };
}

std::string visible_tail(
    std::string_view value,
    std::size_t max_characters) {
    if (value.size() <= max_characters) {
        return std::string(value);
    }
    if (max_characters <= 3U) {
        return std::string(
            value.substr(value.size() - max_characters));
    }

    return "..." +
           std::string(
               value.substr(
                   value.size() -
                   (max_characters - 3U)));
}

void draw_field(
    render::UiRenderer& ui,
    const Rect& rect,
    bool active,
    std::string_view label,
    const std::string& value) {
    ui.text(
        rect.x,
        rect.y - 22.0F,
        2.0F,
        label,
        kMuted);

    ui.rectangle(
        rect.x - 2.0F,
        rect.y - 2.0F,
        rect.width + 4.0F,
        rect.height + 4.0F,
        active ? kAccent : kBorder);
    ui.rectangle(
        rect.x,
        rect.y,
        rect.width,
        rect.height,
        kField);

    const auto visible =
        visible_tail(value, 39U);
    ui.text(
        rect.x + 14.0F,
        rect.y + 16.0F,
        2.0F,
        visible.empty() ? "-" : visible,
        value.empty() ? kMuted : kText);
}

void draw_login_form(
    render::UiRenderer& ui,
    const LoginFormState& form,
    const LoginLayout& layout) {
    ui.begin();

    ui.rectangle(
        layout.panel.x - 2.0F,
        layout.panel.y - 2.0F,
        layout.panel.width + 4.0F,
        layout.panel.height + 4.0F,
        kAccent);
    ui.rectangle(
        layout.panel.x,
        layout.panel.y,
        layout.panel.width,
        layout.panel.height,
        kPanel);

    ui.rectangle(
        layout.panel.x + 20.0F,
        layout.panel.y + 20.0F,
        layout.panel.width - 40.0F,
        104.0F,
        kPanelInner);

    ui.text(
        layout.panel.x + 42.0F,
        layout.panel.y + 42.0F,
        4.0F,
        "OPENGENESISLINK",
        kText);
    ui.text(
        layout.panel.x + 44.0F,
        layout.panel.y + 80.0F,
        2.0F,
        "VIEWER  /  ENTER YOUR WORLD",
        kAccent);
    ui.text(
        layout.panel.x + 44.0F,
        layout.panel.y + 104.0F,
        1.0F,
        "SERVER AUTHORITATIVE  |  SCENE V2  |  OGL1",
        kMuted);

    draw_field(
        ui,
        layout.server,
        form.active_field == LoginField::server,
        "SERVER / CORE URL",
        form.server);
    draw_field(
        ui,
        layout.username,
        form.active_field == LoginField::username,
        "USERNAME",
        form.username);
    draw_field(
        ui,
        layout.password,
        form.active_field == LoginField::password,
        "PASSWORD",
        form.masked_password());
    draw_field(
        ui,
        layout.region,
        form.active_field == LoginField::region,
        "REGION ID",
        form.region);

    const auto button_color =
        form.connecting || !form.complete()
            ? kButtonDisabled
            : kButton;
    ui.rectangle(
        layout.button.x,
        layout.button.y,
        layout.button.width,
        layout.button.height,
        button_color);

    const std::string_view button_text =
        form.connecting
            ? "CONNECTING..."
            : "ENTER WORLD";
    const auto button_text_width =
        ui.text_width(button_text, 2.5F);
    ui.text(
        layout.button.x +
            (layout.button.width - button_text_width) * 0.5F,
        layout.button.y + 19.0F,
        2.5F,
        button_text,
        kText);

    if (!form.error.empty()) {
        ui.text(
            layout.panel.x + 42.0F,
            layout.panel.y + 550.0F,
            1.2F,
            visible_tail(form.error, 68U),
            kError);
    } else if (!form.status.empty()) {
        ui.text(
            layout.panel.x + 42.0F,
            layout.panel.y + 550.0F,
            1.2F,
            visible_tail(form.status, 68U),
            kSuccess);
    } else {
        ui.text(
            layout.panel.x + 42.0F,
            layout.panel.y + 550.0F,
            1.2F,
            "TAB: NEXT FIELD   SHIFT+TAB: BACK   ENTER: CONNECT",
            kMuted);
    }

    ui.render();
}

void process_login_mouse(
    GLFWwindow* window,
    LoginFormState& form,
    bool& submit_requested,
    const LoginLayout& layout,
    bool& previous_pressed,
    int framebuffer_width,
    int framebuffer_height) {
    const bool pressed =
        glfwGetMouseButton(
            window,
            GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    if (!pressed || previous_pressed || form.connecting) {
        previous_pressed = pressed;
        return;
    }
    previous_pressed = pressed;

    double cursor_x = 0.0;
    double cursor_y = 0.0;
    glfwGetCursorPos(
        window,
        &cursor_x,
        &cursor_y);

    int window_width = 0;
    int window_height = 0;
    glfwGetWindowSize(
        window,
        &window_width,
        &window_height);

    if (window_width <= 0 || window_height <= 0) {
        return;
    }

    cursor_x *=
        static_cast<double>(framebuffer_width) /
        static_cast<double>(window_width);
    cursor_y *=
        static_cast<double>(framebuffer_height) /
        static_cast<double>(window_height);

    if (layout.server.contains(cursor_x, cursor_y)) {
        form.focus(LoginField::server);
    } else if (layout.username.contains(cursor_x, cursor_y)) {
        form.focus(LoginField::username);
    } else if (layout.password.contains(cursor_x, cursor_y)) {
        form.focus(LoginField::password);
    } else if (layout.region.contains(cursor_x, cursor_y)) {
        form.focus(LoginField::region);
    } else if (layout.button.contains(cursor_x, cursor_y)) {
        submit_requested = true;
    }
}

void wipe_password(std::string& password) noexcept {
    std::fill(
        password.begin(),
        password.end(),
        '\0');
    password.clear();
}

void draw_world_status(
    render::UiRenderer& ui,
    const ViewerRuntime& runtime,
    bool reconnect_pending) {
    if (!runtime.world_model().initialized()) {
        return;
    }

    const auto background =
        runtime.background_state();
    const auto& info =
        runtime.connection_info();

    ui.begin();
    ui.rectangle(
        14.0F,
        14.0F,
        430.0F,
        112.0F,
        kPanel);

    ui.text(
        28.0F,
        29.0F,
        1.6F,
        "REGION: " + info.region_id,
        kText);

    const auto terrain =
        background.terrain_resolution == 0U
            ? std::string{"LOADING"}
            : std::to_string(
                  background.terrain_resolution) +
                  "X" +
                  std::to_string(
                      background.terrain_resolution);

    ui.text(
        28.0F,
        54.0F,
        1.4F,
        "SEQ: " +
            std::to_string(
                runtime.world_model().sequence()) +
            "  TERRAIN: " + terrain,
        kMuted);

    std::string movement =
        info.avatar_reconcile_supported
            ? "RECONCILE"
            : "UNAVAILABLE";
    if (background.movement_pending) {
        movement = "RECONCILING";
    }
    if (reconnect_pending) {
        movement = "RECONNECTING";
    }

    ui.text(
        28.0F,
        78.0F,
        1.4F,
        "MOVE: " + movement +
            (background.boundary.empty()
                 ? std::string{}
                 : "  EDGE: " +
                       background.boundary),
        reconnect_pending ? kError : kAccent);

    if (!background.last_error.empty()) {
        ui.text(
            28.0F,
            101.0F,
            1.0F,
            visible_tail(
                background.last_error,
                64U),
            kError);
    } else {
        ui.text(
            28.0F,
            101.0F,
            1.0F,
            "WASD MOVE  |  ARROWS LOOK  |  ESC EXIT",
            kMuted);
    }

    ui.render();
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

    const bool graphical_login =
        !options.render_demo &&
        !options.live_login.has_value();

    const std::string initial_title =
        std::string{"OpenGenesisLINK Viewer "} +
        OGL_VIEWER_VERSION +
        (options.render_demo
             ? " - Render Demo"
             : options.live_login.has_value()
                   ? " - Connecting"
                   : " - Login");

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

        render::UiRenderer ui_renderer;
        ui_renderer.initialize();

        input::CameraState camera;
        camera.position = {128.0, 116.0, 28.0};
        camera.yaw_degrees = 90.0;
        camera.pitch_degrees = -12.0;
        camera.move_speed = 12.0;
        input::CameraController camera_controller;

        auto demo_region = make_demo_region();
        ViewerRuntime live_runtime;

        LoginFormState login_form;
        login_form.status =
            "ENTER SERVER, USERNAME, PASSWORD AND REGION";
        bool show_login = graphical_login;
        bool submit_requested = false;
        bool previous_mouse_pressed = false;

        LoginInputContext login_input{
            .form = &login_form,
            .visible = &show_login,
            .submit_requested = &submit_requested,
        };
        glfwSetWindowUserPointer(
            window,
            &login_input);
        glfwSetCharCallback(
            window,
            &login_character_callback);
        glfwSetKeyCallback(
            window,
            &login_key_callback);

        bool live_mode =
            options.live_login.has_value();
        std::future<ConnectionInfo> connection_future;
        bool connection_pending = false;

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
        double next_avatar_command = previous_time;
        bool reconnect_pending = false;
        bool avatar_was_moving = false;

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

            int width = 0;
            int height = 0;
            glfwGetFramebufferSize(window, &width, &height);
            if (width != previous_width || height != previous_height) {
                renderer.resize(width, height);
                ui_renderer.resize(width, height);
                previous_width = width;
                previous_height = height;
            }

            const auto layout =
                login_layout(width, height);

            if (show_login) {
                process_login_mouse(
                    window,
                    login_form,
                    submit_requested,
                    layout,
                    previous_mouse_pressed,
                    width,
                    height);
            }

            if (show_login &&
                submit_requested &&
                !connection_pending &&
                !login_form.connecting) {
                submit_requested = false;

                if (!login_form.complete()) {
                    login_form.error =
                        "ALL FOUR LOGIN FIELDS ARE REQUIRED";
                } else {
                    auto request = login_form.request();
                    login_form.clear_password();
                    login_form.error.clear();
                    login_form.status =
                        "DISCOVERING SERVER AND AUTHENTICATING";
                    login_form.connecting = true;
                    glfwSetWindowTitle(
                        window,
                        "OpenGenesisLINK Viewer - Connecting...");

                    connection_future = std::async(
                        std::launch::async,
                        [&live_runtime,
                         request = std::move(request)]() mutable {
                            try {
                                auto info =
                                    live_runtime.connect(request);
                                wipe_password(request.password);
                                return info;
                            } catch (...) {
                                wipe_password(request.password);
                                throw;
                            }
                        });
                    connection_pending = true;
                }
            }

            if (connection_pending &&
                connection_future.valid() &&
                connection_future.wait_for(
                    std::chrono::seconds(0)) ==
                    std::future_status::ready) {
                connection_pending = false;
                login_form.connecting = false;

                try {
                    const auto info =
                        connection_future.get();
                    login_form.error.clear();
                    login_form.status =
                        "CONNECTED TO " + info.region_id;
                    show_login = false;
                    live_mode = true;
                    reconnect_pending = false;
                    next_scene_poll = current_time + 0.5;
                    next_avatar_command = current_time;
                    avatar_was_moving = false;
                    position_camera_at_spawn(camera, info);

                    const auto title =
                        connected_title(info);
                    glfwSetWindowTitle(
                        window,
                        title.c_str());

                    std::cout
                        << "Connected to OpenGenesisLINK "
                        << info.server_version
                        << " region " << info.region_id
                        << " as " << info.username
                        << "\n";
                } catch (const std::exception& ex) {
                    live_mode = false;
                    login_form.status.clear();
                    login_form.error =
                        std::string{"LOGIN FAILED: "} +
                        ex.what();
                    glfwSetWindowTitle(
                        window,
                        "OpenGenesisLINK Viewer - Login");
                }
            }

            if (live_mode &&
                !connection_pending) {
                if (live_runtime.connected() &&
                    current_time >= next_scene_poll) {
                    try {
                        const auto synchronized =
                            live_runtime.poll(256U);
                        (void)synchronized;
                        next_scene_poll =
                            current_time + 0.5;
                    } catch (const std::exception& ex) {
                        std::cerr
                            << "Scene synchronization failed: "
                            << ex.what()
                            << "\n";
                        reconnect_pending = true;
                        next_reconnect_attempt =
                            current_time + 1.0;
                        glfwSetWindowTitle(
                            window,
                            "OpenGenesisLINK Viewer - Connection lost");
                    }
                }

                if (reconnect_pending &&
                    current_time >= next_reconnect_attempt) {
                    try {
                        const auto info =
                            live_runtime.reconnect();
                        reconnect_pending = false;
                        next_scene_poll =
                            current_time + 0.5;
                        const auto title =
                            connected_title(info);
                        glfwSetWindowTitle(
                            window,
                            title.c_str());
                        std::cout
                            << "Scene connection resumed at sequence "
                            << live_runtime.world_model().sequence()
                            << "\n";
                    } catch (const std::exception& ex) {
                        std::cerr
                            << "Scene reconnect failed: "
                            << ex.what()
                            << "\n";
                        next_reconnect_attempt =
                            current_time + 5.0;
                        glfwSetWindowTitle(
                            window,
                            "OpenGenesisLINK Viewer - Reconnecting...");
                    }
                }
            }

            if (!show_login &&
                !connection_pending) {
                if (live_mode &&
                    live_runtime.connected() &&
                    !reconnect_pending) {
                    const auto camera_input =
                        read_camera_look_input(
                            window,
                            delta_seconds);
                    camera_controller.update(
                        camera,
                        camera_input,
                        delta_seconds);

                    const auto avatar_input =
                        read_avatar_control(
                            window,
                            camera);
                    if (current_time >=
                            next_avatar_command &&
                        (avatar_input.moving ||
                         avatar_was_moving)) {
                        (void)live_runtime
                            .queue_avatar_control(
                                avatar_input.control);
                        next_avatar_command =
                            current_time + 0.1;
                        avatar_was_moving =
                            avatar_input.moving;
                    }

                    live_runtime.service_background();
                    follow_avatar_camera(
                        camera,
                        live_runtime);
                } else if (!live_mode) {
                    const auto camera_input =
                        read_camera_input(
                            window,
                            delta_seconds);
                    camera_controller.update(
                        camera,
                        camera_input,
                        delta_seconds);
                }
            }

            const world::RenderRegion* render_region =
                nullptr;
            if (live_mode &&
                live_runtime.world_model().initialized()) {
                render_region =
                    &live_runtime.render_region();
            } else if (options.render_demo) {
                render_region = &demo_region;
            }

            renderer.render(
                render_region,
                camera);

            if (show_login) {
                draw_login_form(
                    ui_renderer,
                    login_form,
                    layout);
            } else if (live_mode &&
                       live_runtime.world_model().initialized()) {
                draw_world_status(
                    ui_renderer,
                    live_runtime,
                    reconnect_pending);
            }

            glfwSwapBuffers(window);
        }

        if (connection_pending &&
            connection_future.valid()) {
            try {
                (void)connection_future.get();
            } catch (...) {
            }
        }

        login_form.clear_password();
    }

    return 0;
}

} // namespace ogl::viewer::app
