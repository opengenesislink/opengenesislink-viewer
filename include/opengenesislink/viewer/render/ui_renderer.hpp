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

    void text(
        float x,
        float y,
        float scale,
        std::string_view value,
        UiColor color);

    [[nodiscard]] float text_width(
        std::string_view value,
        float scale) const noexcept;

    void render();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ogl::viewer::render
