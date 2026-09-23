#include "opengenesislink/viewer/render/ui_renderer.hpp"

#include <GL/glew.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ogl::viewer::render {
namespace {

struct UiVertex {
    float x = 0.0F;
    float y = 0.0F;
    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;
    float a = 1.0F;
};

GLuint compile_shader(GLenum type, const char* source) {
    const auto shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_TRUE) {
        return shader;
    }

    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::string log(
        static_cast<std::size_t>(length > 0 ? length : 1),
        '\0');
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    glDeleteShader(shader);
    throw std::runtime_error(
        "UI shader compile failed: " + log);
}

GLuint build_program() {
    constexpr const char* vertex_source = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec4 aColor;
uniform vec2 uViewport;
out vec4 vColor;
void main() {
    vec2 ndc = vec2(
        (aPosition.x / uViewport.x) * 2.0 - 1.0,
        1.0 - (aPosition.y / uViewport.y) * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    vColor = aColor;
}
)GLSL";

    constexpr const char* fragment_source = R"GLSL(
#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
}
)GLSL";

    const auto vertex =
        compile_shader(GL_VERTEX_SHADER, vertex_source);
    const auto fragment =
        compile_shader(GL_FRAGMENT_SHADER, fragment_source);

    const auto program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok == GL_TRUE) {
        return program;
    }

    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    std::string log(
        static_cast<std::size_t>(length > 0 ? length : 1),
        '\0');
    glGetProgramInfoLog(program, length, nullptr, log.data());
    glDeleteProgram(program);
    throw std::runtime_error(
        "UI shader link failed: " + log);
}

using Glyph = std::array<std::uint8_t, 7>;

Glyph glyph_for(char input) {
    const auto upper = static_cast<char>(
        std::toupper(static_cast<unsigned char>(input)));

    switch (upper) {
    case 'A': return {0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001};
    case 'B': return {0b11110,0b10001,0b10001,0b11110,0b10001,0b10001,0b11110};
    case 'C': return {0b01111,0b10000,0b10000,0b10000,0b10000,0b10000,0b01111};
    case 'D': return {0b11110,0b10001,0b10001,0b10001,0b10001,0b10001,0b11110};
    case 'E': return {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b11111};
    case 'F': return {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b10000};
    case 'G': return {0b01111,0b10000,0b10000,0b10111,0b10001,0b10001,0b01111};
    case 'H': return {0b10001,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001};
    case 'I': return {0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b11111};
    case 'J': return {0b00111,0b00010,0b00010,0b00010,0b10010,0b10010,0b01100};
    case 'K': return {0b10001,0b10010,0b10100,0b11000,0b10100,0b10010,0b10001};
    case 'L': return {0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111};
    case 'M': return {0b10001,0b11011,0b10101,0b10101,0b10001,0b10001,0b10001};
    case 'N': return {0b10001,0b11001,0b10101,0b10011,0b10001,0b10001,0b10001};
    case 'O': return {0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110};
    case 'P': return {0b11110,0b10001,0b10001,0b11110,0b10000,0b10000,0b10000};
    case 'Q': return {0b01110,0b10001,0b10001,0b10001,0b10101,0b10010,0b01101};
    case 'R': return {0b11110,0b10001,0b10001,0b11110,0b10100,0b10010,0b10001};
    case 'S': return {0b01111,0b10000,0b10000,0b01110,0b00001,0b00001,0b11110};
    case 'T': return {0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100};
    case 'U': return {0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110};
    case 'V': return {0b10001,0b10001,0b10001,0b10001,0b10001,0b01010,0b00100};
    case 'W': return {0b10001,0b10001,0b10001,0b10101,0b10101,0b10101,0b01010};
    case 'X': return {0b10001,0b10001,0b01010,0b00100,0b01010,0b10001,0b10001};
    case 'Y': return {0b10001,0b10001,0b01010,0b00100,0b00100,0b00100,0b00100};
    case 'Z': return {0b11111,0b00001,0b00010,0b00100,0b01000,0b10000,0b11111};
    case '0': return {0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b01110};
    case '1': return {0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110};
    case '2': return {0b01110,0b10001,0b00001,0b00010,0b00100,0b01000,0b11111};
    case '3': return {0b11110,0b00001,0b00001,0b01110,0b00001,0b00001,0b11110};
    case '4': return {0b00010,0b00110,0b01010,0b10010,0b11111,0b00010,0b00010};
    case '5': return {0b11111,0b10000,0b10000,0b11110,0b00001,0b00001,0b11110};
    case '6': return {0b01110,0b10000,0b10000,0b11110,0b10001,0b10001,0b01110};
    case '7': return {0b11111,0b00001,0b00010,0b00100,0b01000,0b01000,0b01000};
    case '8': return {0b01110,0b10001,0b10001,0b01110,0b10001,0b10001,0b01110};
    case '9': return {0b01110,0b10001,0b10001,0b01111,0b00001,0b00001,0b01110};
    case ' ': return {0,0,0,0,0,0,0};
    case '.': return {0,0,0,0,0,0,0b00100};
    case ':': return {0,0b00100,0,0,0b00100,0,0};
    case '/': return {0b00001,0b00010,0b00100,0b01000,0b10000,0,0};
    case '-': return {0,0,0,0b11111,0,0,0};
    case '_': return {0,0,0,0,0,0,0b11111};
    case '@': return {0b01110,0b10001,0b10111,0b10101,0b10111,0b10000,0b01111};
    case '*': return {0,0b10101,0b01110,0b11111,0b01110,0b10101,0};
    case '?': return {0b01110,0b10001,0b00001,0b00010,0b00100,0,0b00100};
    case '=': return {0,0b11111,0,0b11111,0,0,0};
    case '+': return {0,0b00100,0b00100,0b11111,0b00100,0b00100,0};
    case '#': return {0b01010,0b11111,0b01010,0b01010,0b11111,0b01010,0};
    case '[': return {0b01110,0b01000,0b01000,0b01000,0b01000,0b01000,0b01110};
    case ']': return {0b01110,0b00010,0b00010,0b00010,0b00010,0b00010,0b01110};
    case '(': return {0b00010,0b00100,0b01000,0b01000,0b01000,0b00100,0b00010};
    case ')': return {0b01000,0b00100,0b00010,0b00010,0b00010,0b00100,0b01000};
    case '|': return {0b00100,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100};
    default: return {0b11111,0b10001,0b10001,0b10101,0b10001,0b10001,0b11111};
    }
}

void append_rectangle(
    std::vector<UiVertex>& vertices,
    float x,
    float y,
    float width,
    float height,
    UiColor color) {
    if (width <= 0.0F || height <= 0.0F) {
        return;
    }

    const UiVertex a{x, y, color.r, color.g, color.b, color.a};
    const UiVertex b{x + width, y, color.r, color.g, color.b, color.a};
    const UiVertex c{x + width, y + height, color.r, color.g, color.b, color.a};
    const UiVertex d{x, y + height, color.r, color.g, color.b, color.a};

    vertices.insert(
        vertices.end(),
        {a, b, c, a, c, d});
}

} // namespace

struct UiRenderer::Impl {
    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLint viewport_location = -1;
    int width = 1280;
    int height = 720;
    bool initialized = false;
    std::vector<UiVertex> vertices;

    void destroy() noexcept {
        if (vbo != 0U) glDeleteBuffers(1, &vbo);
        if (vao != 0U) glDeleteVertexArrays(1, &vao);
        if (program != 0U) glDeleteProgram(program);
        vbo = 0;
        vao = 0;
        program = 0;
        initialized = false;
        vertices.clear();
    }
};

UiRenderer::UiRenderer()
    : impl_(std::make_unique<Impl>()) {}

UiRenderer::~UiRenderer() {
    if (impl_) {
        impl_->destroy();
    }
}

UiRenderer::UiRenderer(UiRenderer&&) noexcept = default;
UiRenderer& UiRenderer::operator=(UiRenderer&&) noexcept = default;

void UiRenderer::initialize() {
    if (impl_->initialized) {
        return;
    }

    impl_->program = build_program();
    impl_->viewport_location =
        glGetUniformLocation(
            impl_->program,
            "uViewport");
    if (impl_->viewport_location < 0) {
        impl_->destroy();
        throw std::runtime_error(
            "UI viewport shader uniform is unavailable");
    }

    glGenVertexArrays(1, &impl_->vao);
    glGenBuffers(1, &impl_->vbo);

    glBindVertexArray(impl_->vao);
    glBindBuffer(GL_ARRAY_BUFFER, impl_->vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        static_cast<GLsizei>(sizeof(UiVertex)),
        reinterpret_cast<const void*>(
            offsetof(UiVertex, x)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        4,
        GL_FLOAT,
        GL_FALSE,
        static_cast<GLsizei>(sizeof(UiVertex)),
        reinterpret_cast<const void*>(
            offsetof(UiVertex, r)));

    glBindVertexArray(0);
    impl_->initialized = true;
}

void UiRenderer::resize(int width, int height) {
    impl_->width = width > 0 ? width : 1;
    impl_->height = height > 0 ? height : 1;
}

void UiRenderer::begin() {
    impl_->vertices.clear();
}

void UiRenderer::rectangle(
    float x,
    float y,
    float width,
    float height,
    UiColor color) {
    append_rectangle(
        impl_->vertices,
        x,
        y,
        width,
        height,
        color);
}

void UiRenderer::text(
    float x,
    float y,
    float scale,
    std::string_view value,
    UiColor color) {
    if (scale <= 0.0F) {
        return;
    }

    const auto start_x = x;
    for (const char character : value) {
        if (character == '\n') {
            x = start_x;
            y += 9.0F * scale;
            continue;
        }

        const auto glyph = glyph_for(character);
        for (std::size_t row = 0U; row < glyph.size(); ++row) {
            for (std::size_t column = 0U; column < 5U; ++column) {
                const auto mask =
                    static_cast<std::uint8_t>(
                        1U << (4U - column));
                if ((glyph[row] & mask) == 0U) {
                    continue;
                }

                append_rectangle(
                    impl_->vertices,
                    x + static_cast<float>(column) * scale,
                    y + static_cast<float>(row) * scale,
                    scale,
                    scale,
                    color);
            }
        }
        x += 6.0F * scale;
    }
}

float UiRenderer::text_width(
    std::string_view value,
    float scale) const noexcept {
    std::size_t current = 0U;
    std::size_t longest = 0U;

    for (const char character : value) {
        if (character == '\n') {
            longest = std::max(longest, current);
            current = 0U;
        } else {
            ++current;
        }
    }
    longest = std::max(longest, current);

    return static_cast<float>(longest) *
           6.0F *
           scale;
}

void UiRenderer::render() {
    if (!impl_->initialized) {
        throw std::runtime_error(
            "UI renderer is not initialized");
    }
    if (impl_->vertices.empty()) {
        return;
    }
    if (impl_->vertices.size() >
        static_cast<std::size_t>(
            std::numeric_limits<GLsizei>::max())) {
        throw std::runtime_error(
            "UI vertex count exceeds OpenGL draw range");
    }

    glUseProgram(impl_->program);
    glUniform2f(
        impl_->viewport_location,
        static_cast<float>(impl_->width),
        static_cast<float>(impl_->height));

    glBindVertexArray(impl_->vao);
    glBindBuffer(GL_ARRAY_BUFFER, impl_->vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(
            impl_->vertices.size() *
            sizeof(UiVertex)),
        impl_->vertices.data(),
        GL_DYNAMIC_DRAW);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawArrays(
        GL_TRIANGLES,
        0,
        static_cast<GLsizei>(
            impl_->vertices.size()));

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    glBindVertexArray(0);
    glUseProgram(0);
}

} // namespace ogl::viewer::render
