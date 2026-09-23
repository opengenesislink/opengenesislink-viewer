#pragma once

#include "opengenesislink/viewer/app/login_flow.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace ogl::viewer::app {

enum class LoginField {
    server,
    username,
    password,
    region
};

class LoginFormState {
public:
    std::string server;
    std::string username;
    std::string password;
    std::string region;
    std::string status;
    std::string error;
    LoginField active_field = LoginField::server;
    bool connecting = false;

    void focus(LoginField field) noexcept;
    void focus_next(bool reverse = false) noexcept;
    void append_ascii(char value);
    void backspace();

    [[nodiscard]] bool complete() const noexcept;
    [[nodiscard]] LoginRequest request() const;
    [[nodiscard]] std::string masked_password() const;

    void clear_password() noexcept;

private:
    [[nodiscard]] std::string& active_value() noexcept;
    [[nodiscard]] const std::string& active_value() const noexcept;
    [[nodiscard]] std::size_t active_limit() const noexcept;
};

} // namespace ogl::viewer::app
