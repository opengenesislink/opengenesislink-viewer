#include "opengenesislink/viewer/render/opengl_renderer.hpp"

#include <GL/glew.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace ogl::viewer::render {
namespace {

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
    throw std::runtime_error("OpenGL shader compile failed: " + log);
}

GLuint build_program() {
    constexpr const char* vertex_source = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPosition;
uniform mat4 uMvp;
void main() {
    gl_Position = uMvp * vec4(aPosition, 1.0);
}
)GLSL";

    constexpr const char* fragment_source = R"GLSL(
#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() {
    FragColor = uColor;
}
)GLSL";

    const auto vertex = compile_shader(GL_VERTEX_SHADER, vertex_source);
    const auto fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_source);

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
    throw std::runtime_error("OpenGL shader link failed: " + log);
}

glm::mat4 world_transform(const world::Transform& transform) {
    auto matrix = glm::mat4{1.0F};
    matrix = glm::translate(
        matrix,
        glm::vec3{
            static_cast<float>(transform.position.x),
            static_cast<float>(transform.position.y),
            static_cast<float>(transform.position.z)});
    matrix = glm::rotate(
        matrix,
        glm::radians(static_cast<float>(transform.rotation.z)),
        glm::vec3{0.0F, 0.0F, 1.0F});
    matrix = glm::rotate(
        matrix,
        glm::radians(static_cast<float>(transform.rotation.y)),
        glm::vec3{0.0F, 1.0F, 0.0F});
    matrix = glm::rotate(
        matrix,
        glm::radians(static_cast<float>(transform.rotation.x)),
        glm::vec3{1.0F, 0.0F, 0.0F});
    return glm::scale(
        matrix,
        glm::vec3{
            static_cast<float>(transform.scale.x),
            static_cast<float>(transform.scale.y),
            static_cast<float>(transform.scale.z)});
}

glm::mat4 view_projection(
    const input::CameraState& camera,
    int width,
    int height) {
    constexpr float pi = 3.14159265358979323846F;
    const auto yaw =
        static_cast<float>(camera.yaw_degrees) * pi / 180.0F;
    const auto pitch =
        static_cast<float>(camera.pitch_degrees) * pi / 180.0F;

    const glm::vec3 position{
        static_cast<float>(camera.position.x),
        static_cast<float>(camera.position.y),
        static_cast<float>(camera.position.z)};
    const glm::vec3 direction{
        std::cos(pitch) * std::cos(yaw),
        std::cos(pitch) * std::sin(yaw),
        std::sin(pitch)};

    const auto safe_height = height > 0 ? height : 1;
    const auto aspect =
        static_cast<float>(width > 0 ? width : 1) /
        static_cast<float>(safe_height);

    const auto projection =
        glm::perspective(glm::radians(60.0F), aspect, 0.05F, 4096.0F);
    const auto view = glm::lookAt(
        position,
        position + direction,
        glm::vec3{0.0F, 0.0F, 1.0F});
    return projection * view;
}

} // namespace

struct OpenGlRenderer::Impl {
    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLint mvp_location = -1;
    GLint color_location = -1;
    int width = 1280;
    int height = 720;
    bool initialized = false;

    void destroy() noexcept {
        if (ebo != 0U) glDeleteBuffers(1, &ebo);
        if (vbo != 0U) glDeleteBuffers(1, &vbo);
        if (vao != 0U) glDeleteVertexArrays(1, &vao);
        if (program != 0U) glDeleteProgram(program);
        ebo = 0;
        vbo = 0;
        vao = 0;
        program = 0;
        initialized = false;
    }
};

OpenGlRenderer::OpenGlRenderer()
    : impl_(std::make_unique<Impl>()) {}

OpenGlRenderer::~OpenGlRenderer() {
    if (impl_) {
        impl_->destroy();
    }
}

OpenGlRenderer::OpenGlRenderer(OpenGlRenderer&&) noexcept = default;
OpenGlRenderer& OpenGlRenderer::operator=(OpenGlRenderer&&) noexcept = default;

void OpenGlRenderer::initialize() {
    if (impl_->initialized) {
        return;
    }

    glewExperimental = GL_TRUE;
    const auto glew_result = glewInit();
    if (glew_result != GLEW_OK) {
        throw std::runtime_error(
            "GLEW initialization failed: " +
            std::string{
                reinterpret_cast<const char*>(
                    glewGetErrorString(glew_result))});
    }

    // GLEW may generate GL_INVALID_ENUM on core contexts.
    while (glGetError() != GL_NO_ERROR) {
    }

    impl_->program = build_program();
    impl_->mvp_location =
        glGetUniformLocation(impl_->program, "uMvp");
    impl_->color_location =
        glGetUniformLocation(impl_->program, "uColor");
    if (impl_->mvp_location < 0 || impl_->color_location < 0) {
        impl_->destroy();
        throw std::runtime_error("OpenGL shader uniforms are unavailable");
    }

    constexpr std::array<float, 24> vertices{
        -0.5F, -0.5F, -0.5F,
         0.5F, -0.5F, -0.5F,
         0.5F,  0.5F, -0.5F,
        -0.5F,  0.5F, -0.5F,
        -0.5F, -0.5F,  0.5F,
         0.5F, -0.5F,  0.5F,
         0.5F,  0.5F,  0.5F,
        -0.5F,  0.5F,  0.5F};

    constexpr std::array<std::uint32_t, 36> indices{
        0U, 1U, 2U, 2U, 3U, 0U,
        4U, 6U, 5U, 6U, 4U, 7U,
        0U, 4U, 5U, 5U, 1U, 0U,
        3U, 2U, 6U, 6U, 7U, 3U,
        0U, 3U, 7U, 7U, 4U, 0U,
        1U, 5U, 6U, 6U, 2U, 1U};

    glGenVertexArrays(1, &impl_->vao);
    glGenBuffers(1, &impl_->vbo);
    glGenBuffers(1, &impl_->ebo);

    glBindVertexArray(impl_->vao);
    glBindBuffer(GL_ARRAY_BUFFER, impl_->vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
        vertices.data(),
        GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, impl_->ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(
            indices.size() * sizeof(std::uint32_t)),
        indices.data(),
        GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        static_cast<GLsizei>(3U * sizeof(float)),
        nullptr);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    impl_->initialized = true;
}

void OpenGlRenderer::resize(int width, int height) {
    impl_->width = width > 0 ? width : 1;
    impl_->height = height > 0 ? height : 1;
    glViewport(0, 0, impl_->width, impl_->height);
}

void OpenGlRenderer::render(
    const world::RenderRegion* region,
    const input::CameraState& camera) {
    if (!impl_->initialized) {
        throw std::runtime_error("OpenGL renderer is not initialized");
    }

    glClearColor(0.015F, 0.045F, 0.095F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (region == nullptr) {
        return;
    }

    const auto vp =
        view_projection(camera, impl_->width, impl_->height);

    glUseProgram(impl_->program);
    glBindVertexArray(impl_->vao);

    if (region->terrain_width > 0U &&
        region->terrain_height > 0U &&
        region->terrain_cell_size > 0.0) {
        world::Transform water;
        water.position = {
            static_cast<double>(region->terrain_width) *
                region->terrain_cell_size * 0.5,
            static_cast<double>(region->terrain_height) *
                region->terrain_cell_size * 0.5,
            region->water_height - 0.025};
        water.scale = {
            static_cast<double>(region->terrain_width) *
                region->terrain_cell_size,
            static_cast<double>(region->terrain_height) *
                region->terrain_cell_size,
            0.05};

        const auto mvp = vp * world_transform(water);
        glUniformMatrix4fv(
            impl_->mvp_location,
            1,
            GL_FALSE,
            &mvp[0][0]);
        glUniform4f(
            impl_->color_location,
            0.02F,
            0.28F,
            0.58F,
            1.0F);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
    }

    for (const auto& [id, instance] : region->instances) {
        (void)id;
        const auto mvp = vp * world_transform(instance.transform);
        glUniformMatrix4fv(
            impl_->mvp_location,
            1,
            GL_FALSE,
            &mvp[0][0]);

        if (instance.geometry == world::RenderGeometry::avatar_capsule) {
            glUniform4f(
                impl_->color_location,
                0.25F,
                0.85F,
                1.0F,
                1.0F);
        } else if (instance.physical) {
            glUniform4f(
                impl_->color_location,
                0.18F,
                0.55F,
                1.0F,
                1.0F);
        } else {
            glUniform4f(
                impl_->color_location,
                0.72F,
                0.82F,
                0.95F,
                1.0F);
        }

        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
    }

    glBindVertexArray(0);
    glUseProgram(0);
}

} // namespace ogl::viewer::render
