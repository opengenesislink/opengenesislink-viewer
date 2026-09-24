#include "opengenesislink/viewer/app/desktop_application.hpp"

#include "opengenesislink/viewer/app/content_inspector.hpp"
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
    Rect screen;
    Rect top_bar;
    Rect panel;
    Rect left_feature;
    Rect right_news;
    Rect footer;
    Rect tab_login;
    Rect tab_grids;
    Rect tab_settings;
    Rect tab_advanced;
    Rect server;
    Rect region;
    Rect username;
    Rect password;
    Rect button;
    float scale = 1.0F;
};

struct ViewerInputContext {
    LoginFormState* login_form = nullptr;
    bool* login_visible = nullptr;
    bool* login_submit_requested = nullptr;
    std::string* console_input = nullptr;
    bool* console_visible = nullptr;
    bool* console_submit_requested = nullptr;
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
constexpr render::UiColor kBackgroundTop{
    0.005F, 0.018F, 0.045F, 1.0F};
constexpr render::UiColor kBackgroundBottom{
    0.008F, 0.045F, 0.085F, 1.0F};
constexpr render::UiColor kTopBar{
    0.01F, 0.035F, 0.07F, 0.97F};
constexpr render::UiColor kGlass{
    0.018F, 0.055F, 0.105F, 0.92F};
constexpr render::UiColor kGlassSoft{
    0.025F, 0.075F, 0.13F, 0.84F};
constexpr render::UiColor kLine{
    0.16F, 0.42F, 0.72F, 0.55F};
constexpr render::UiColor kPlanet{
    0.02F, 0.12F, 0.24F, 0.78F};
constexpr render::UiColor kPlanetEdge{
    0.07F, 0.28F, 0.52F, 0.42F};

void glfw_error_callback(int code, const char* description) {
    std::cerr << "GLFW error " << code << ": "
              << (description != nullptr ? description : "unknown")
              << "\n";
}

void login_character_callback(
    GLFWwindow* window,
    unsigned int codepoint) {
    auto* context = static_cast<ViewerInputContext*>(
        glfwGetWindowUserPointer(window));
    if (context == nullptr ||
        codepoint > 126U) {
        return;
    }

    if (context->login_form != nullptr &&
        context->login_visible != nullptr &&
        *context->login_visible &&
        !context->login_form->connecting) {
        context->login_form->append_ascii(
            static_cast<char>(codepoint));
        return;
    }

    if (context->console_input != nullptr &&
        context->console_visible != nullptr &&
        *context->console_visible &&
        codepoint >= 32U &&
        context->console_input->size() < 2048U) {
        context->console_input->push_back(
            static_cast<char>(codepoint));
    }
}

void login_key_callback(
    GLFWwindow* window,
    int key,
    int,
    int action,
    int mods) {
    auto* context = static_cast<ViewerInputContext*>(
        glfwGetWindowUserPointer(window));
    if (context == nullptr ||
        (action != GLFW_PRESS &&
         action != GLFW_REPEAT)) {
        return;
    }

    if (context->login_form != nullptr &&
        context->login_visible != nullptr &&
        context->login_submit_requested != nullptr &&
        *context->login_visible &&
        !context->login_form->connecting) {
        if (key == GLFW_KEY_BACKSPACE) {
            context->login_form->backspace();
            return;
        }
        if (key == GLFW_KEY_TAB &&
            action == GLFW_PRESS) {
            context->login_form->focus_next(
                (mods & GLFW_MOD_SHIFT) != 0);
            return;
        }
        if ((key == GLFW_KEY_ENTER ||
             key == GLFW_KEY_KP_ENTER) &&
            action == GLFW_PRESS) {
            *context->login_submit_requested = true;
        }
        return;
    }

    if (context->console_input != nullptr &&
        context->console_visible != nullptr &&
        context->console_submit_requested != nullptr &&
        *context->console_visible) {
        if (key == GLFW_KEY_BACKSPACE) {
            if (!context->console_input->empty()) {
                context->console_input->pop_back();
            }
            return;
        }
        if ((key == GLFW_KEY_ENTER ||
             key == GLFW_KEY_KP_ENTER) &&
            action == GLFW_PRESS) {
            *context->console_submit_requested = true;
            return;
        }
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
    const auto scale =
        std::clamp(
            std::min(
                safe_width / 1600.0F,
                safe_height / 900.0F),
            0.72F,
            1.35F);

    const auto panel_width = 640.0F * scale;
    const auto panel_height = 430.0F * scale;
    const auto panel_x =
        (safe_width - panel_width) * 0.5F;
    const auto panel_y = 270.0F * scale;

    const auto inner_x = panel_x + 30.0F * scale;
    const auto inner_width = panel_width - 60.0F * scale;
    const auto row_gap = 12.0F * scale;
    const auto region_width = 150.0F * scale;
    const auto server_width =
        inner_width - region_width - row_gap;
    const auto field_height = 44.0F * scale;

    const auto side_width = 420.0F * scale;
    const auto side_height = 260.0F * scale;
    const auto side_y = 535.0F * scale;
    const auto side_margin = 20.0F * scale;

    const auto footer_height = 48.0F * scale;
    const auto tab_width = panel_width / 4.0F;
    const auto tab_height = 46.0F * scale;

    return {
        .screen = {
            0.0F,
            0.0F,
            safe_width,
            safe_height,
        },
        .top_bar = {
            0.0F,
            0.0F,
            safe_width,
            48.0F * scale,
        },
        .panel = {
            panel_x,
            panel_y,
            panel_width,
            panel_height,
        },
        .left_feature = {
            side_margin,
            side_y,
            side_width,
            side_height,
        },
        .right_news = {
            safe_width - side_margin - side_width,
            side_y - 8.0F * scale,
            side_width,
            side_height + 8.0F * scale,
        },
        .footer = {
            0.0F,
            safe_height - footer_height,
            safe_width,
            footer_height,
        },
        .tab_login = {
            panel_x,
            panel_y,
            tab_width,
            tab_height,
        },
        .tab_grids = {
            panel_x + tab_width,
            panel_y,
            tab_width,
            tab_height,
        },
        .tab_settings = {
            panel_x + tab_width * 2.0F,
            panel_y,
            tab_width,
            tab_height,
        },
        .tab_advanced = {
            panel_x + tab_width * 3.0F,
            panel_y,
            tab_width,
            tab_height,
        },
        .server = {
            inner_x,
            panel_y + 78.0F * scale,
            server_width,
            field_height,
        },
        .region = {
            inner_x + server_width + row_gap,
            panel_y + 78.0F * scale,
            region_width,
            field_height,
        },
        .username = {
            inner_x,
            panel_y + 151.0F * scale,
            inner_width,
            field_height,
        },
        .password = {
            inner_x,
            panel_y + 220.0F * scale,
            inner_width,
            field_height,
        },
        .button = {
            inner_x + 82.0F * scale,
            panel_y + 306.0F * scale,
            inner_width - 164.0F * scale,
            52.0F * scale,
        },
        .scale = scale,
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

void draw_outline(
    render::UiRenderer& ui,
    const Rect& rect,
    float thickness,
    render::UiColor color) {
    ui.rectangle(
        rect.x,
        rect.y,
        rect.width,
        thickness,
        color);
    ui.rectangle(
        rect.x,
        rect.y + rect.height - thickness,
        rect.width,
        thickness,
        color);
    ui.rectangle(
        rect.x,
        rect.y,
        thickness,
        rect.height,
        color);
    ui.rectangle(
        rect.x + rect.width - thickness,
        rect.y,
        thickness,
        rect.height,
        color);
}

void draw_field(
    render::UiRenderer& ui,
    const Rect& rect,
    bool active,
    std::string_view label,
    const std::string& value,
    std::string_view placeholder,
    float scale) {
    ui.text(
        rect.x,
        rect.y - 17.0F * scale,
        1.15F * scale,
        label,
        kText);

    ui.rectangle(
        rect.x,
        rect.y,
        rect.width,
        rect.height,
        kField);
    draw_outline(
        ui,
        rect,
        std::max(1.0F, 1.4F * scale),
        active ? kAccent : kBorder);

    const auto visible =
        visible_tail(value, 46U);
    const auto shown =
        visible.empty()
            ? std::string{placeholder}
            : visible;
    ui.text(
        rect.x + 14.0F * scale,
        rect.y + 15.0F * scale,
        1.25F * scale,
        shown,
        visible.empty() ? kMuted : kText);
}

void draw_login_form(
    render::UiRenderer& ui,
    const LoginFormState& form,
    const LoginLayout& layout) {
    const auto s = layout.scale;
    const auto width = layout.screen.width;
    const auto height = layout.screen.height;

    ui.begin();

    ui.vertical_gradient(
        0.0F,
        0.0F,
        width,
        height,
        kBackgroundTop,
        kBackgroundBottom);

    ui.circle(
        80.0F * s,
        250.0F * s,
        255.0F * s,
        kPlanetEdge,
        96U);
    ui.circle(
        80.0F * s,
        250.0F * s,
        244.0F * s,
        kPlanet,
        96U);
    ui.circle(
        405.0F * s,
        128.0F * s,
        40.0F * s,
        render::UiColor{
            0.07F, 0.17F, 0.31F, 0.9F},
        48U);

    ui.rectangle(
        0.0F,
        430.0F * s,
        width,
        2.0F * s,
        render::UiColor{
            0.06F, 0.28F, 0.5F, 0.28F});
    ui.vertical_gradient(
        0.0F,
        430.0F * s,
        width,
        250.0F * s,
        render::UiColor{
            0.01F, 0.08F, 0.15F, 0.08F},
        render::UiColor{
            0.005F, 0.02F, 0.045F, 0.9F});

    ui.rectangle(
        layout.top_bar.x,
        layout.top_bar.y,
        layout.top_bar.width,
        layout.top_bar.height,
        kTopBar);
    ui.rectangle(
        0.0F,
        layout.top_bar.height - 1.0F,
        width,
        1.0F,
        kLine);
    ui.text(
        22.0F * s,
        17.0F * s,
        1.45F * s,
        "OPENGENESISLINK VIEWER",
        kText);
    ui.text(
        265.0F * s,
        18.0F * s,
        1.0F * s,
        std::string{"V"} + OGL_VIEWER_VERSION,
        kMuted);

    ui.text(
        44.0F * s,
        120.0F * s,
        1.55F * s,
        "MORE THAN A WORLD...",
        kText);
    ui.text(
        64.0F * s,
        146.0F * s,
        1.35F * s,
        "A NEW BEGINNING.",
        kMuted);

    const auto title_left = std::string_view{"OPENGENESIS"};
    const auto title_right = std::string_view{"LINK"};
    const auto title_scale = 4.6F * s;
    const auto title_width =
        ui.text_width(title_left, title_scale) +
        ui.text_width(title_right, title_scale);
    const auto title_x = (width - title_width) * 0.5F;
    ui.text(
        title_x,
        92.0F * s,
        title_scale,
        title_left,
        kText);
    ui.text(
        title_x +
            ui.text_width(title_left, title_scale),
        92.0F * s,
        title_scale,
        title_right,
        kAccent);

    const auto viewer_label = std::string_view{"V I E W E R"};
    const auto viewer_scale = 2.1F * s;
    ui.text(
        (width -
         ui.text_width(
             viewer_label,
             viewer_scale)) *
            0.5F,
        145.0F * s,
        viewer_scale,
        viewer_label,
        kText);

    const auto claim =
        std::string_view{
            "EXPLORE  -  CREATE  -  CONNECT  -  BELONG"};
    ui.text(
        (width -
         ui.text_width(
             claim,
             1.25F * s)) *
            0.5F,
        184.0F * s,
        1.25F * s,
        claim,
        kMuted);
    const auto subclaim =
        std::string_view{
            "A VIRTUAL WORLD, REBUILT FROM THE CORE."};
    ui.text(
        (width -
         ui.text_width(
             subclaim,
             1.0F * s)) *
            0.5F,
        211.0F * s,
        1.0F * s,
        subclaim,
        kText);

    ui.rectangle(
        width - 335.0F * s,
        85.0F * s,
        275.0F * s,
        102.0F * s,
        render::UiColor{
            0.005F, 0.04F, 0.09F, 0.38F});
    ui.text(
        width - 310.0F * s,
        105.0F * s,
        1.4F * s,
        "REAL PEOPLE.",
        kText);
    ui.text(
        width - 310.0F * s,
        132.0F * s,
        1.4F * s,
        "ENDLESS POSSIBILITIES.",
        kAccent);

    ui.rectangle(
        layout.panel.x,
        layout.panel.y,
        layout.panel.width,
        layout.panel.height,
        kGlass);
    draw_outline(
        ui,
        layout.panel,
        std::max(1.0F, 1.0F * s),
        render::UiColor{
            0.12F, 0.28F, 0.46F, 0.85F});

    ui.rectangle(
        layout.tab_login.x,
        layout.tab_login.y,
        layout.tab_login.width,
        layout.tab_login.height,
        kPanelInner);
    ui.rectangle(
        layout.tab_grids.x,
        layout.tab_grids.y,
        layout.tab_grids.width,
        layout.tab_grids.height,
        kGlassSoft);
    ui.rectangle(
        layout.tab_settings.x,
        layout.tab_settings.y,
        layout.tab_settings.width,
        layout.tab_settings.height,
        kGlassSoft);
    ui.rectangle(
        layout.tab_advanced.x,
        layout.tab_advanced.y,
        layout.tab_advanced.width,
        layout.tab_advanced.height,
        kGlassSoft);
    ui.rectangle(
        layout.tab_login.x,
        layout.tab_login.y +
            layout.tab_login.height -
            3.0F * s,
        layout.tab_login.width,
        3.0F * s,
        kAccent);

    ui.text(
        layout.tab_login.x + 46.0F * s,
        layout.tab_login.y + 16.0F * s,
        1.15F * s,
        "LOGIN",
        kText);
    ui.text(
        layout.tab_grids.x + 46.0F * s,
        layout.tab_grids.y + 16.0F * s,
        1.15F * s,
        "GRIDS",
        kMuted);
    ui.text(
        layout.tab_settings.x + 25.0F * s,
        layout.tab_settings.y + 16.0F * s,
        1.05F * s,
        "EINSTELLUNGEN",
        kMuted);
    ui.text(
        layout.tab_advanced.x + 34.0F * s,
        layout.tab_advanced.y + 16.0F * s,
        1.05F * s,
        "ERWEITERT",
        kMuted);

    draw_field(
        ui,
        layout.server,
        form.active_field == LoginField::server,
        "GRID / WELT",
        form.server,
        "CORE URL",
        s);
    draw_field(
        ui,
        layout.region,
        form.active_field == LoginField::region,
        "STARTREGION",
        form.region,
        "REGION",
        s);
    draw_field(
        ui,
        layout.username,
        form.active_field == LoginField::username,
        "BENUTZERNAME",
        form.username,
        "BENUTZERNAME ODER E-MAIL",
        s);
    draw_field(
        ui,
        layout.password,
        form.active_field == LoginField::password,
        "PASSWORT",
        form.masked_password(),
        "PASSWORT",
        s);

    ui.rectangle(
        layout.panel.x + 30.0F * s,
        layout.panel.y + 281.0F * s,
        16.0F * s,
        16.0F * s,
        kAccent);
    ui.text(
        layout.panel.x + 55.0F * s,
        layout.panel.y + 282.0F * s,
        0.95F * s,
        "ZUGANGSDATEN SPEICHERN",
        kText);
    draw_outline(
        ui,
        Rect{
            layout.panel.x + 360.0F * s,
            layout.panel.y + 281.0F * s,
            16.0F * s,
            16.0F * s},
        std::max(1.0F, s),
        kBorder);
    ui.text(
        layout.panel.x + 385.0F * s,
        layout.panel.y + 282.0F * s,
        0.95F * s,
        "BEIM START EINLOGGEN",
        kMuted);

    const auto button_color =
        form.connecting || !form.complete()
            ? kButtonDisabled
            : kButton;
    ui.vertical_gradient(
        layout.button.x,
        layout.button.y,
        layout.button.width,
        layout.button.height,
        button_color,
        render::UiColor{
            button_color.r * 0.72F,
            button_color.g * 0.82F,
            std::min(1.0F, button_color.b * 1.05F),
            button_color.a});
    const std::string_view button_text =
        form.connecting
            ? "VERBINDUNG WIRD HERGESTELLT..."
            : "EINLOGGEN IN DIE WELT";
    const auto button_text_width =
        ui.text_width(
            button_text,
            1.55F * s);
    ui.text(
        layout.button.x +
            (layout.button.width -
             button_text_width) *
                0.5F,
        layout.button.y + 18.0F * s,
        1.55F * s,
        button_text,
        kText);

    ui.text(
        layout.panel.x + 88.0F * s,
        layout.panel.y + 372.0F * s,
        0.9F * s,
        "ACCOUNT ERSTELLEN  |  PASSWORT VERGESSEN?  |  GRID HINZUFUEGEN",
        kAccent);

    if (!form.error.empty()) {
        ui.text(
            layout.panel.x + 30.0F * s,
            layout.panel.y + 405.0F * s,
            0.9F * s,
            visible_tail(form.error, 86U),
            kError);
    } else if (!form.status.empty()) {
        ui.text(
            layout.panel.x + 30.0F * s,
            layout.panel.y + 405.0F * s,
            0.9F * s,
            visible_tail(form.status, 86U),
            kSuccess);
    }

    ui.rectangle(
        layout.left_feature.x,
        layout.left_feature.y,
        layout.left_feature.width,
        layout.left_feature.height,
        kGlassSoft);
    draw_outline(
        ui,
        layout.left_feature,
        std::max(1.0F, s),
        render::UiColor{
            0.12F, 0.29F, 0.48F, 0.85F});
    ui.vertical_gradient(
        layout.left_feature.x + 10.0F * s,
        layout.left_feature.y + 10.0F * s,
        layout.left_feature.width - 20.0F * s,
        145.0F * s,
        render::UiColor{
            0.03F, 0.16F, 0.28F, 0.88F},
        render::UiColor{
            0.015F, 0.07F, 0.12F, 0.92F});
    ui.text(
        layout.left_feature.x + 24.0F * s,
        layout.left_feature.y + 35.0F * s,
        1.15F * s,
        "VIRTUELLE WELTEN",
        kAccent);
    ui.text(
        layout.left_feature.x + 24.0F * s,
        layout.left_feature.y + 64.0F * s,
        2.05F * s,
        "ENTDECKEN",
        kText);
    ui.text(
        layout.left_feature.x + 24.0F * s,
        layout.left_feature.y + 101.0F * s,
        1.0F * s,
        "DEINE REISE BEGINNT HIER.",
        kMuted);
    ui.text(
        layout.left_feature.x + 24.0F * s,
        layout.left_feature.y + 182.0F * s,
        1.6F * s,
        "OPEN WORLDS",
        kText);
    ui.text(
        layout.left_feature.x + 24.0F * s,
        layout.left_feature.y + 215.0F * s,
        0.95F * s,
        "FREI  -  OFFEN  -  VERBUNDEN",
        kMuted);

    ui.rectangle(
        layout.right_news.x,
        layout.right_news.y,
        layout.right_news.width,
        layout.right_news.height,
        kGlassSoft);
    draw_outline(
        ui,
        layout.right_news,
        std::max(1.0F, s),
        render::UiColor{
            0.12F, 0.29F, 0.48F, 0.85F});
    ui.text(
        layout.right_news.x + 24.0F * s,
        layout.right_news.y + 26.0F * s,
        1.55F * s,
        "NEUIGKEITEN",
        kText);
    ui.text(
        layout.right_news.x + 285.0F * s,
        layout.right_news.y + 28.0F * s,
        0.85F * s,
        "ALLE ANZEIGEN >",
        kAccent);
    ui.rectangle(
        layout.right_news.x + 24.0F * s,
        layout.right_news.y + 62.0F * s,
        82.0F * s,
        50.0F * s,
        render::UiColor{
            0.05F, 0.17F, 0.29F, 1.0F});
    ui.text(
        layout.right_news.x + 122.0F * s,
        layout.right_news.y + 68.0F * s,
        1.0F * s,
        "0.13.0-ALPHA.1",
        kText);
    ui.text(
        layout.right_news.x + 122.0F * s,
        layout.right_news.y + 91.0F * s,
        0.82F * s,
        "ERSTER TESTBUILD BEREIT",
        kMuted);
    ui.rectangle(
        layout.right_news.x + 24.0F * s,
        layout.right_news.y + 126.0F * s,
        82.0F * s,
        50.0F * s,
        render::UiColor{
            0.04F, 0.13F, 0.23F, 1.0F});
    ui.text(
        layout.right_news.x + 122.0F * s,
        layout.right_news.y + 132.0F * s,
        1.0F * s,
        "ALPHA TEST",
        kText);
    ui.text(
        layout.right_news.x + 122.0F * s,
        layout.right_news.y + 155.0F * s,
        0.82F * s,
        "LOGIN, WORLD, SOCIAL, BUILD",
        kMuted);
    ui.rectangle(
        layout.right_news.x + 24.0F * s,
        layout.right_news.y + 190.0F * s,
        82.0F * s,
        50.0F * s,
        render::UiColor{
            0.035F, 0.11F, 0.2F, 1.0F});
    ui.text(
        layout.right_news.x + 122.0F * s,
        layout.right_news.y + 196.0F * s,
        1.0F * s,
        "ROADMAP",
        kText);
    ui.text(
        layout.right_news.x + 122.0F * s,
        layout.right_news.y + 219.0F * s,
        0.82F * s,
        "ATLAS UND VISUAL ASSETS ALS NAECHSTES",
        kMuted);

    const auto feature_y =
        std::min(
            height - 103.0F * s,
            layout.panel.y +
                layout.panel.height +
                34.0F * s);
    const auto feature_start =
        width * 0.5F - 300.0F * s;
    constexpr std::array<std::string_view, 5> feature_titles{
        "MENSCHEN",
        "WELTEN",
        "BAUEN",
        "HANDEL",
        "EVENTS",
    };
    for (std::size_t index = 0U;
         index < feature_titles.size();
         ++index) {
        const auto x =
            feature_start +
            static_cast<float>(index) *
                150.0F * s;
        draw_outline(
            ui,
            Rect{
                x + 32.0F * s,
                feature_y,
                42.0F * s,
                42.0F * s},
            std::max(1.0F, 1.5F * s),
            index == 2U ? kAccent : kText);
        ui.text(
            x,
            feature_y + 55.0F * s,
            0.85F * s,
            feature_titles[index],
            index == 2U ? kAccent : kText);
    }

    ui.rectangle(
        layout.footer.x,
        layout.footer.y,
        layout.footer.width,
        layout.footer.height,
        kTopBar);
    ui.rectangle(
        0.0F,
        layout.footer.y,
        width,
        1.0F,
        kLine);
    ui.text(
        24.0F * s,
        layout.footer.y + 17.0F * s,
        0.9F * s,
        "SPRACHE: DEUTSCH  |  BARRIEREFREIHEIT  |  SUPPORT  |  WEBSEITE",
        kMuted);
    const auto powered =
        std::string_view{
            "POWERED BY NEXVORTEX.DE  |  OPENGENESISLINK"};
    ui.text(
        width -
            ui.text_width(
                powered,
                0.9F * s) -
            24.0F * s,
        layout.footer.y + 17.0F * s,
        0.9F * s,
        powered,
        kAccent);

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
        500.0F,
        158.0F,
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

    ui.text(
        28.0F,
        101.0F,
        1.1F,
        "APPEARANCE: " +
            std::to_string(
                background.appearance_revision) +
            "  INVENTORY: " +
            std::to_string(
                background.inventory_folders) +
            "F/" +
            std::to_string(
                background.inventory_items) +
            "I",
        kMuted);

    ui.text(
        28.0F,
        123.0F,
        1.1F,
        "ASSETS: " +
            std::to_string(
                background.asset_cached) +
            "/" +
            std::to_string(
                background.asset_dependencies) +
            (background.asset_prefetch_pending
                 ? "  PREFETCH"
                 : "  READY"),
        background.asset_prefetch_pending
            ? kAccent
            : kSuccess);

    if (!background.last_error.empty()) {
        ui.text(
            28.0F,
            145.0F,
            1.0F,
            visible_tail(
                background.last_error,
                76U),
            kError);
    } else {
        ui.text(
            28.0F,
            145.0F,
            1.0F,
            "WASD MOVE  |  ARROWS LOOK  |  ESC EXIT",
            kMuted);
    }

    ui.render();
}

void draw_content_inspector(
    render::UiRenderer& ui,
    const ViewerRuntime& runtime,
    ContentInspectorSection section,
    std::size_t page,
    int framebuffer_width,
    int framebuffer_height) {
    const auto view =
        build_content_inspector_view(
            runtime.bootstrap_content(),
            section,
            page,
            12U);

    const auto safe_width =
        static_cast<float>(
            std::max(framebuffer_width, 1));
    const auto safe_height =
        static_cast<float>(
            std::max(framebuffer_height, 1));

    const auto panel_width =
        std::clamp(
            safe_width - 80.0F,
            620.0F,
            880.0F);
    const auto panel_height =
        std::clamp(
            safe_height - 120.0F,
            430.0F,
            610.0F);
    const auto panel_x =
        (safe_width - panel_width) * 0.5F;
    const auto panel_y =
        (safe_height - panel_height) * 0.5F;

    ui.begin();
    ui.rectangle(
        panel_x - 2.0F,
        panel_y - 2.0F,
        panel_width + 4.0F,
        panel_height + 4.0F,
        kAccent);
    ui.rectangle(
        panel_x,
        panel_y,
        panel_width,
        panel_height,
        kPanel);

    ui.rectangle(
        panel_x + 18.0F,
        panel_y + 18.0F,
        panel_width - 36.0F,
        76.0F,
        kPanelInner);

    ui.text(
        panel_x + 34.0F,
        panel_y + 34.0F,
        2.5F,
        "CONTENT INSPECTOR / " +
            view.title,
        kText);

    ui.text(
        panel_x + 34.0F,
        panel_y + 67.0F,
        1.2F,
        "APPEARANCE  |  INVENTORY  |  ASSETS",
        kMuted);

    float line_y = panel_y + 118.0F;
    for (const auto& line : view.lines) {
        ui.text(
            panel_x + 34.0F,
            line_y,
            1.25F,
            visible_tail(line, 92U),
            kText);
        line_y += 28.0F;
    }

    const auto page_text =
        "PAGE " +
        std::to_string(view.page + 1U) +
        "/" +
        std::to_string(view.page_count);

    ui.text(
        panel_x + 34.0F,
        panel_y + panel_height - 48.0F,
        1.15F,
        page_text,
        kAccent);

    ui.text(
        panel_x + 190.0F,
        panel_y + panel_height - 48.0F,
        1.05F,
        "I CLOSE  |  TAB SECTION  |  PGUP PGDN PAGE",
        kMuted);

    ui.render();
}

std::vector<std::string> display_lines(
    std::string_view value,
    std::size_t max_lines) {
    std::vector<std::string> lines;
    std::size_t start = 0U;
    while (start <= value.size()) {
        const auto end =
            value.find('\n', start);
        lines.emplace_back(
            value.substr(
                start,
                end == std::string_view::npos
                    ? std::string_view::npos
                    : end - start));
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1U;
    }

    if (lines.size() > max_lines) {
        lines.erase(
            lines.begin(),
            lines.begin() +
                static_cast<std::ptrdiff_t>(
                    lines.size() - max_lines));
    }
    return lines;
}

void draw_chat_and_command_bar(
    render::UiRenderer& ui,
    const ViewerRuntime& runtime,
    bool command_visible,
    const std::string& command_input,
    int framebuffer_width,
    int framebuffer_height) {
    const auto width =
        static_cast<float>(
            std::max(framebuffer_width, 1));
    const auto height =
        static_cast<float>(
            std::max(framebuffer_height, 1));

    const auto panel_width =
        std::min(width - 28.0F, 760.0F);
    const auto x = 14.0F;
    const auto input_height =
        command_visible ? 54.0F : 28.0F;
    const auto bottom = height - 18.0F;
    const auto input_y =
        bottom - input_height;

    ui.begin();

    const auto& chat =
        runtime.world_model().chat_history();
    const auto chat_count =
        std::min<std::size_t>(
            6U,
            chat.size());
    if (chat_count > 0U) {
        const auto chat_height =
            static_cast<float>(
                chat_count) *
                22.0F +
            18.0F;
        const auto chat_y =
            input_y - chat_height - 8.0F;
        ui.rectangle(
            x,
            chat_y,
            panel_width,
            chat_height,
            kPanel);

        const auto first =
            chat.size() - chat_count;
        float line_y = chat_y + 10.0F;
        for (std::size_t index = first;
             index < chat.size();
             ++index) {
            const auto& message = chat[index];
            const auto prefix =
                message.kind == "chat_whisper"
                    ? "[WHISPER] "
                    : message.kind == "chat_shout"
                          ? "[SHOUT] "
                          : "";
            ui.text(
                x + 12.0F,
                line_y,
                1.0F,
                visible_tail(
                    prefix +
                        message.sender_name +
                        ": " +
                        message.text,
                    108U),
                kText);
            line_y += 22.0F;
        }
    }

    const auto command =
        runtime.command_state();
    const auto result_lines =
        display_lines(
            command.last_result,
            5U);
    if (!result_lines.empty()) {
        const auto result_height =
            static_cast<float>(
                result_lines.size()) *
                20.0F +
            14.0F;
        const auto result_y =
            input_y -
            (chat_count > 0U
                 ? static_cast<float>(
                       chat_count) *
                       22.0F +
                       40.0F
                 : 8.0F) -
            result_height;
        ui.rectangle(
            x,
            result_y,
            panel_width,
            result_height,
            kPanel);
        float line_y =
            result_y + 8.0F;
        for (const auto& line :
             result_lines) {
            ui.text(
                x + 12.0F,
                line_y,
                0.95F,
                visible_tail(line, 112U),
                command.pending
                    ? kAccent
                    : kMuted);
            line_y += 20.0F;
        }
    }

    if (command_visible) {
        ui.rectangle(
            x - 2.0F,
            input_y - 2.0F,
            panel_width + 4.0F,
            input_height + 4.0F,
            kAccent);
        ui.rectangle(
            x,
            input_y,
            panel_width,
            input_height,
            kPanelInner);
        ui.text(
            x + 12.0F,
            input_y + 8.0F,
            1.0F,
            "CHAT / VIEWER COMMAND  |  /HELP",
            kMuted);
        ui.text(
            x + 12.0F,
            input_y + 29.0F,
            1.2F,
            "> " +
                visible_tail(
                    command_input,
                    98U),
            kText);
    } else {
        ui.rectangle(
            x,
            input_y,
            330.0F,
            input_height,
            kPanel);
        ui.text(
            x + 10.0F,
            input_y + 9.0F,
            1.0F,
            "ENTER: CHAT / COMMANDS   I: CONTENT",
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
        1600,
        900,
        initial_title.c_str(),
        nullptr,
        nullptr);
    if (window == nullptr) {
        throw std::runtime_error("Unable to create Viewer window");
    }

    glfwSetWindowSizeLimits(
        window,
        1100,
        680,
        GLFW_DONT_CARE,
        GLFW_DONT_CARE);

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
            "SERVER, ZUGANGSDATEN UND STARTREGION EINGEBEN";
        bool show_login = graphical_login;
        bool submit_requested = false;
        bool previous_mouse_pressed = false;

        std::string command_input;
        bool command_visible = false;
        bool command_submit_requested = false;

        ViewerInputContext input_context{
            .login_form = &login_form,
            .login_visible = &show_login,
            .login_submit_requested = &submit_requested,
            .console_input = &command_input,
            .console_visible = &command_visible,
            .console_submit_requested =
                &command_submit_requested,
        };
        glfwSetWindowUserPointer(
            window,
            &input_context);
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

        bool content_inspector_visible = false;
        ContentInspectorSection content_inspector_section =
            ContentInspectorSection::appearance;
        std::size_t content_inspector_page = 0U;
        bool previous_inspector_key = false;
        bool previous_section_key = false;
        bool previous_page_up_key = false;
        bool previous_page_down_key = false;
        bool previous_enter_key = false;
        bool previous_escape_key = false;

        int previous_width = 0;
        int previous_height = 0;

        while (glfwWindowShouldClose(window) == GLFW_FALSE) {
            glfwPollEvents();

            const bool escape_key =
                glfwGetKey(window, GLFW_KEY_ESCAPE) ==
                GLFW_PRESS;
            if (escape_key &&
                !previous_escape_key) {
                if (command_visible) {
                    command_visible = false;
                    command_input.clear();
                } else if (content_inspector_visible) {
                    content_inspector_visible = false;
                } else {
                    glfwSetWindowShouldClose(
                        window,
                        GLFW_TRUE);
                }
            }
            previous_escape_key = escape_key;

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

            const bool inspector_key =
                glfwGetKey(window, GLFW_KEY_I) ==
                GLFW_PRESS;
            const bool section_key =
                glfwGetKey(window, GLFW_KEY_TAB) ==
                GLFW_PRESS;
            const bool page_up_key =
                glfwGetKey(window, GLFW_KEY_PAGE_UP) ==
                GLFW_PRESS;
            const bool page_down_key =
                glfwGetKey(window, GLFW_KEY_PAGE_DOWN) ==
                GLFW_PRESS;
            const bool enter_key =
                glfwGetKey(window, GLFW_KEY_ENTER) ==
                    GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_KP_ENTER) ==
                    GLFW_PRESS;

            if (!show_login &&
                live_mode &&
                live_runtime.world_model().initialized()) {
                if (!command_visible &&
                    enter_key &&
                    !previous_enter_key) {
                    command_visible = true;
                    content_inspector_visible = false;
                    command_input.clear();

                    if (avatar_was_moving &&
                        live_runtime.connected()) {
                        input::AvatarControlInput stop;
                        stop.heading_degrees =
                            camera.yaw_degrees;
                        stop.speed = 4.0;
                        stop.command_seconds = 0.1;
                        (void)live_runtime
                            .queue_avatar_control(stop);
                        avatar_was_moving = false;
                    }
                }

                if (!command_visible &&
                    inspector_key &&
                    !previous_inspector_key) {
                    content_inspector_visible =
                        !content_inspector_visible;

                    if (content_inspector_visible &&
                        avatar_was_moving &&
                        live_runtime.connected()) {
                        input::AvatarControlInput stop;
                        stop.heading_degrees =
                            camera.yaw_degrees;
                        stop.speed = 4.0;
                        stop.command_seconds = 0.1;
                        (void)live_runtime
                            .queue_avatar_control(stop);
                        avatar_was_moving = false;
                    }
                }

                if (!command_visible &&
                    content_inspector_visible &&
                    section_key &&
                    !previous_section_key) {
                    content_inspector_section =
                        next_content_inspector_section(
                            content_inspector_section);
                    content_inspector_page = 0U;
                }

                if (!command_visible &&
                    content_inspector_visible &&
                    page_up_key &&
                    !previous_page_up_key &&
                    content_inspector_page > 0U) {
                    --content_inspector_page;
                }

                if (!command_visible &&
                    content_inspector_visible &&
                    page_down_key &&
                    !previous_page_down_key) {
                    ++content_inspector_page;
                }
            }

            if (command_visible &&
                command_submit_requested) {
                command_submit_requested = false;
                if (!command_input.empty()) {
                    (void)live_runtime.submit_command(
                        command_input);
                    command_input.clear();
                    command_visible = false;
                }
            }

            previous_inspector_key =
                inspector_key;
            previous_section_key =
                section_key;
            previous_page_up_key =
                page_up_key;
            previous_page_down_key =
                page_down_key;
            previous_enter_key =
                enter_key;

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
                    content_inspector_visible = false;
                    content_inspector_page = 0U;
                    command_visible = false;
                    command_input.clear();
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
                    if (!content_inspector_visible &&
                        !command_visible) {
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

                if (content_inspector_visible) {
                    draw_content_inspector(
                        ui_renderer,
                        live_runtime,
                        content_inspector_section,
                        content_inspector_page,
                        width,
                        height);
                }

                draw_chat_and_command_bar(
                    ui_renderer,
                    live_runtime,
                    command_visible,
                    command_input,
                    width,
                    height);
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
