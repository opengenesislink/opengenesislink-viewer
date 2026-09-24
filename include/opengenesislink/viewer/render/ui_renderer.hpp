#pragma once

#include <memory>
#include <string_view>

namespace ogl::viewer::render {

struct UiColor {
    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;
    float a = 1.0F;
};

class UiRenderer {
public:
    UiRenderer();
    ~UiRenderer();

    UiRenderer(const UiRenderer&) = delete;
    UiRenderer& operator=(const UiRenderer&) = delete;
    UiRenderer(UiRenderer&&) noexcept;
    UiRenderer& operator=(UiRenderer&&) noexcept;

    void initialize();
    void resize(int width, int height);
    void begin();

    void rectangle(
        float x,
        float y,
        float width,
        float height,
        UiColor color);

    void vertical_gradient(
        float x,
        float y,
        float width,
        float height,
        UiColor top,
        UiColor bottom);

    void circle(
        float center_x,
        float center_y,
        float radius,
        UiColor color,
        unsigned int segments = 64U);

    void triangle(
        float ax,
        float ay,
        float bx,
        float by,
        float cx,
        float cy,
        UiColor color);

    void text(
        float x,
        float y,
        float scale,
        std::string_view value,
        UiColor color);

    void modern_text(
        float x,
        float y,
        float scale,
        std::string_view value,
        UiColor color);

    [[nodiscard]] float text_width(
        std::string_view value,
        float scale) const noexcept;

    [[nodiscard]] float modern_text_width(
        std::string_view value,
        float scale) const noexcept;

    void render();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ogl::viewer::render
