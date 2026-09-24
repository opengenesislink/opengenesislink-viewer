#include "opengenesislink/viewer/render/ui_renderer.hpp"

#include <GL/glew.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cmath>
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

struct TextVertex {
    float x = 0.0F;
    float y = 0.0F;
    float u = 0.0F;
    float v = 0.0F;
    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;
    float a = 1.0F;
};

struct ModernGlyph {
    int width = 0;
    int height = 0;
    int bearing_x = 0;
    int bearing_y = 0;
    float advance = 0.0F;
    float u0 = 0.0F;
    float v0 = 0.0F;
    float u1 = 0.0F;
    float v1 = 0.0F;
    std::vector<std::uint8_t> alpha;
};

constexpr float kModernFontPixelSize = 48.0F;
constexpr float kModernOutputHeight = 13.0F;
constexpr int kFontAtlasWidth = 1024;
constexpr int kFontAtlasHeight = 256;

bool load_modern_font(
    std::array<ModernGlyph, 128>& glyphs) {
    FT_Library library = nullptr;
    if (FT_Init_FreeType(&library) != 0) {
        return false;
    }

    FT_Face face = nullptr;
#ifdef _WIN32
    constexpr std::array<const char*, 3> candidates{
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/calibri.ttf",
    };
#else
    constexpr std::array<const char*, 4> candidates{
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
    };
#endif

    for (const auto* candidate : candidates) {
        if (FT_New_Face(
                library,
                candidate,
                0,
                &face) == 0) {
            break;
        }
    }

    if (face == nullptr) {
        FT_Done_FreeType(library);
        return false;
    }

    if (FT_Set_Pixel_Sizes(
            face,
            0U,
            static_cast<FT_UInt>(
                kModernFontPixelSize)) != 0) {
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return false;
    }

    for (unsigned int code = 32U;
         code <= 126U;
         ++code) {
        if (FT_Load_Char(
                face,
                code,
                FT_LOAD_RENDER) != 0) {
            continue;
        }

        const auto& bitmap =
            face->glyph->bitmap;
        auto& glyph = glyphs[code];
        glyph.width =
            static_cast<int>(bitmap.width);
        glyph.height =
            static_cast<int>(bitmap.rows);
        glyph.bearing_x =
            face->glyph->bitmap_left;
        glyph.bearing_y =
            face->glyph->bitmap_top;
        glyph.advance =
            static_cast<float>(
                face->glyph->advance.x) /
            64.0F;

        const auto pixel_count =
            static_cast<std::size_t>(
                std::max(glyph.width, 0)) *
            static_cast<std::size_t>(
                std::max(glyph.height, 0));
        glyph.alpha.assign(
            pixel_count,
            static_cast<std::uint8_t>(0U));

        if (glyph.width <= 0 ||
            glyph.height <= 0 ||
            bitmap.buffer == nullptr) {
            continue;
        }

        const auto pitch =
            static_cast<int>(bitmap.pitch);
        const auto stride =
            pitch >= 0 ? pitch : -pitch;

        for (int row = 0;
             row < glyph.height;
             ++row) {
            const auto source_row =
                pitch >= 0
                    ? row
                    : glyph.height - 1 - row;
            const auto* source =
                bitmap.buffer +
                static_cast<std::ptrdiff_t>(
                    source_row) *
                    static_cast<std::ptrdiff_t>(
                        stride);

            for (int column = 0;
                 column < glyph.width;
                 ++column) {
                glyph.alpha[
                    static_cast<std::size_t>(row) *
                        static_cast<std::size_t>(
                            glyph.width) +
                    static_cast<std::size_t>(
                        column)] =
                    source[column];
            }
        }
    }

    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return true;
}

GLuint build_font_atlas(
    std::array<ModernGlyph, 128>& glyphs) {
    std::vector<std::uint8_t> atlas(
        static_cast<std::size_t>(kFontAtlasWidth) *
            static_cast<std::size_t>(kFontAtlasHeight),
        static_cast<std::uint8_t>(0U));

    int pen_x = 1;
    int pen_y = 1;
    int row_height = 0;

    for (unsigned int code = 32U;
         code <= 126U;
         ++code) {
        auto& glyph = glyphs[code];

        if (glyph.width <= 0 ||
            glyph.height <= 0 ||
            glyph.alpha.empty()) {
            continue;
        }

        if (pen_x + glyph.width + 1 >
            kFontAtlasWidth) {
            pen_x = 1;
            pen_y += row_height + 1;
            row_height = 0;
        }

        if (pen_y + glyph.height + 1 >
            kFontAtlasHeight) {
            return 0U;
        }

        for (int row = 0;
             row < glyph.height;
             ++row) {
            const auto source_row =
                glyph.height - 1 - row;
            for (int column = 0;
                 column < glyph.width;
                 ++column) {
                atlas[
                    static_cast<std::size_t>(
                        pen_y + row) *
                        static_cast<std::size_t>(
                            kFontAtlasWidth) +
                    static_cast<std::size_t>(
                        pen_x + column)] =
                    glyph.alpha[
                        static_cast<std::size_t>(
                            source_row) *
                            static_cast<std::size_t>(
                                glyph.width) +
                        static_cast<std::size_t>(
                            column)];
            }
        }

        glyph.u0 =
            static_cast<float>(pen_x) /
            static_cast<float>(kFontAtlasWidth);
        glyph.v0 =
            static_cast<float>(pen_y) /
            static_cast<float>(kFontAtlasHeight);
        glyph.u1 =
            static_cast<float>(
                pen_x + glyph.width) /
            static_cast<float>(kFontAtlasWidth);
        glyph.v1 =
            static_cast<float>(
                pen_y + glyph.height) /
            static_cast<float>(kFontAtlasHeight);

        pen_x += glyph.width + 1;
        row_height =
            std::max(row_height, glyph.height);
    }

    GLuint texture = 0U;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_R8,
        kFontAtlasWidth,
        kFontAtlasHeight,
        0,
        GL_RED,
        GL_UNSIGNED_BYTE,
        atlas.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_LINEAR);
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        GL_LINEAR);
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_S,
        GL_CLAMP_TO_EDGE);
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_T,
        GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    for (auto& glyph : glyphs) {
        glyph.alpha.clear();
        glyph.alpha.shrink_to_fit();
    }

    return texture;
}

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

GLuint build_text_program() {
    constexpr const char* vertex_source = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aColor;
uniform vec2 uViewport;
out vec2 vUv;
out vec4 vColor;
void main() {
    vec2 ndc = vec2(
        (aPosition.x / uViewport.x) * 2.0 - 1.0,
        1.0 - (aPosition.y / uViewport.y) * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    vUv = aUv;
    vColor = aColor;
}
)GLSL";

    constexpr const char* fragment_source = R"GLSL(
#version 330 core
in vec2 vUv;
in vec4 vColor;
uniform sampler2D uGlyphAtlas;
out vec4 FragColor;
void main() {
    float coverage = texture(uGlyphAtlas, vUv).r;
    FragColor = vec4(
        vColor.rgb,
        vColor.a * coverage);
}
)GLSL";

    const auto vertex =
        compile_shader(
            GL_VERTEX_SHADER,
            vertex_source);
    const auto fragment =
        compile_shader(
            GL_FRAGMENT_SHADER,
            fragment_source);

    const auto program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint ok = GL_FALSE;
    glGetProgramiv(
        program,
        GL_LINK_STATUS,
        &ok);
    if (ok == GL_TRUE) {
        return program;
    }

    GLint length = 0;
    glGetProgramiv(
        program,
        GL_INFO_LOG_LENGTH,
        &length);
    std::string log(
        static_cast<std::size_t>(
            length > 0 ? length : 1),
        '\0');
    glGetProgramInfoLog(
        program,
        length,
        nullptr,
        log.data());
    glDeleteProgram(program);
    throw std::runtime_error(
        "UI text shader link failed: " +
        log);
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

void append_vertical_gradient(
    std::vector<UiVertex>& vertices,
    float x,
    float y,
    float width,
    float height,
    UiColor top,
    UiColor bottom) {
    if (width <= 0.0F || height <= 0.0F) {
        return;
    }

    const UiVertex a{x, y, top.r, top.g, top.b, top.a};
    const UiVertex b{x + width, y, top.r, top.g, top.b, top.a};
    const UiVertex c{
        x + width,
        y + height,
        bottom.r,
        bottom.g,
        bottom.b,
        bottom.a};
    const UiVertex d{
        x,
        y + height,
        bottom.r,
        bottom.g,
        bottom.b,
        bottom.a};

    vertices.insert(vertices.end(), {a, b, c, a, c, d});
}

void append_circle(
    std::vector<UiVertex>& vertices,
    float center_x,
    float center_y,
    float radius,
    UiColor color,
    unsigned int segments) {
    if (radius <= 0.0F || segments < 3U) {
        return;
    }

    constexpr float tau = 6.28318530717958647692F;
    const UiVertex center{
        center_x,
        center_y,
        color.r,
        color.g,
        color.b,
        color.a};

    for (unsigned int index = 0U; index < segments; ++index) {
        const auto angle_a =
            tau * static_cast<float>(index) /
            static_cast<float>(segments);
        const auto angle_b =
            tau * static_cast<float>(index + 1U) /
            static_cast<float>(segments);

        const UiVertex a{
            center_x + std::cos(angle_a) * radius,
            center_y + std::sin(angle_a) * radius,
            color.r,
            color.g,
            color.b,
            color.a};
        const UiVertex b{
            center_x + std::cos(angle_b) * radius,
            center_y + std::sin(angle_b) * radius,
            color.r,
            color.g,
            color.b,
            color.a};
        vertices.insert(vertices.end(), {center, a, b});
    }
}

} // namespace

struct UiRenderer::Impl {
    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLint viewport_location = -1;

    GLuint text_program = 0;
    GLuint text_vao = 0;
    GLuint text_vbo = 0;
    GLuint font_atlas = 0;
    GLint text_viewport_location = -1;
    GLint text_sampler_location = -1;

    int width = 1280;
    int height = 720;
    bool initialized = false;
    bool modern_font_ready = false;
    std::array<ModernGlyph, 128> modern_glyphs{};
    std::vector<UiVertex> vertices;
    std::vector<TextVertex> text_vertices;

    void destroy() noexcept {
        if (text_vbo != 0U) {
            glDeleteBuffers(1, &text_vbo);
        }
        if (text_vao != 0U) {
            glDeleteVertexArrays(1, &text_vao);
        }
        if (font_atlas != 0U) {
            glDeleteTextures(1, &font_atlas);
        }
        if (text_program != 0U) {
            glDeleteProgram(text_program);
        }
        if (vbo != 0U) {
            glDeleteBuffers(1, &vbo);
        }
        if (vao != 0U) {
            glDeleteVertexArrays(1, &vao);
        }
        if (program != 0U) {
            glDeleteProgram(program);
        }

        text_vbo = 0U;
        text_vao = 0U;
        font_atlas = 0U;
        text_program = 0U;
        vbo = 0U;
        vao = 0U;
        program = 0U;
        text_viewport_location = -1;
        text_sampler_location = -1;
        viewport_location = -1;
        initialized = false;
        modern_font_ready = false;
        vertices.clear();
        text_vertices.clear();

        for (auto& glyph : modern_glyphs) {
            glyph.alpha.clear();
        }
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

    if (load_modern_font(
            impl_->modern_glyphs)) {
        impl_->font_atlas =
            build_font_atlas(
                impl_->modern_glyphs);
    }

    if (impl_->font_atlas != 0U) {
        impl_->text_program =
            build_text_program();
        impl_->text_viewport_location =
            glGetUniformLocation(
                impl_->text_program,
                "uViewport");
        impl_->text_sampler_location =
            glGetUniformLocation(
                impl_->text_program,
                "uGlyphAtlas");

        if (impl_->text_viewport_location < 0 ||
            impl_->text_sampler_location < 0) {
            impl_->destroy();
            throw std::runtime_error(
                "UI text shader uniforms are unavailable");
        }

        glGenVertexArrays(
            1,
            &impl_->text_vao);
        glGenBuffers(
            1,
            &impl_->text_vbo);

        glBindVertexArray(
            impl_->text_vao);
        glBindBuffer(
            GL_ARRAY_BUFFER,
            impl_->text_vbo);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0,
            2,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(
                sizeof(TextVertex)),
            reinterpret_cast<const void*>(
                offsetof(TextVertex, x)));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,
            2,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(
                sizeof(TextVertex)),
            reinterpret_cast<const void*>(
                offsetof(TextVertex, u)));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(
            2,
            4,
            GL_FLOAT,
            GL_FALSE,
            static_cast<GLsizei>(
                sizeof(TextVertex)),
            reinterpret_cast<const void*>(
                offsetof(TextVertex, r)));

        glBindVertexArray(0);
        impl_->modern_font_ready = true;
    }

    impl_->initialized = true;
}

void UiRenderer::resize(int width, int height) {
    impl_->width = width > 0 ? width : 1;
    impl_->height = height > 0 ? height : 1;
}

void UiRenderer::begin() {
    impl_->vertices.clear();
    impl_->text_vertices.clear();
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

void UiRenderer::vertical_gradient(
    float x,
    float y,
    float width,
    float height,
    UiColor top,
    UiColor bottom) {
    append_vertical_gradient(
        impl_->vertices,
        x,
        y,
        width,
        height,
        top,
        bottom);
}

void UiRenderer::circle(
    float center_x,
    float center_y,
    float radius,
    UiColor color,
    unsigned int segments) {
    append_circle(
        impl_->vertices,
        center_x,
        center_y,
        radius,
        color,
        segments);
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

void UiRenderer::modern_text(
    float x,
    float y,
    float scale,
    std::string_view value,
    UiColor color) {
    if (scale <= 0.0F) {
        return;
    }
    if (!impl_->modern_font_ready) {
        text(
            x,
            y,
            scale,
            value,
            color);
        return;
    }

    const auto unit =
        (kModernOutputHeight * scale) /
        kModernFontPixelSize;
    const auto start_x = x;
    auto baseline =
        y + kModernOutputHeight * scale;

    for (const char raw_character : value) {
        if (raw_character == '\n') {
            x = start_x;
            y += 9.0F * scale;
            baseline =
                y + kModernOutputHeight * scale;
            continue;
        }

        const auto byte =
            static_cast<unsigned char>(
                raw_character);
        const auto code =
            byte >= 32U && byte <= 126U
                ? byte
                : static_cast<unsigned char>('?');
        const auto& glyph =
            impl_->modern_glyphs[code];

        const auto glyph_x =
            x +
            static_cast<float>(
                glyph.bearing_x) *
                unit;
        const auto glyph_y =
            baseline -
            static_cast<float>(
                glyph.bearing_y) *
                unit;

        for (int row = 0;
             row < glyph.height;
             ++row) {
            for (int column = 0;
                 column < glyph.width;
                 ++column) {
                const auto alpha =
                    glyph.alpha[
                        static_cast<std::size_t>(row) *
                            static_cast<std::size_t>(
                                glyph.width) +
                        static_cast<std::size_t>(
                            column)];
                if (alpha < 8U) {
                    continue;
                }

                auto pixel_color = color;
                pixel_color.a *=
                    static_cast<float>(alpha) /
                    255.0F;

                append_rectangle(
                    impl_->vertices,
                    glyph_x +
                        static_cast<float>(
                            column) *
                            unit,
                    glyph_y +
                        static_cast<float>(
                            row) *
                            unit,
                    std::max(unit, 0.35F),
                    std::max(unit, 0.35F),
                    pixel_color);
            }
        }

        x += glyph.advance * unit;
    }
}

float UiRenderer::modern_text_width(
    std::string_view value,
    float scale) const noexcept {
    if (scale <= 0.0F) {
        return 0.0F;
    }
    if (!impl_->modern_font_ready) {
        return text_width(
            value,
            scale);
    }

    const auto unit =
        (kModernOutputHeight * scale) /
        kModernFontPixelSize;
    float current = 0.0F;
    float longest = 0.0F;

    for (const char raw_character : value) {
        if (raw_character == '\n') {
            longest =
                std::max(
                    longest,
                    current);
            current = 0.0F;
            continue;
        }

        const auto byte =
            static_cast<unsigned char>(
                raw_character);
        const auto code =
            byte >= 32U && byte <= 126U
                ? byte
                : static_cast<unsigned char>('?');
        current +=
            impl_->modern_glyphs[code].advance *
            unit;
    }

    return std::max(
        longest,
        current);
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
