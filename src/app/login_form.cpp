#include "opengenesislink/viewer/app/login_form.hpp"

#include <array>
#include <stdexcept>

namespace ogl::viewer::app {

void LoginFormState::focus(LoginField field) noexcept {
    active_field = field;
}

void LoginFormState::focus_next(bool reverse) noexcept {
    constexpr std::array<LoginField, 4> fields{
        LoginField::server,
        LoginField::username,
        LoginField::password,
        LoginField::region,
    };

    std::size_t index = 0U;
    for (std::size_t current = 0U;
         current < fields.size();
         ++current) {
        if (fields[current] == active_field) {
            index = current;
            break;
        }
    }

    if (reverse) {
        index = index == 0U
            ? fields.size() - 1U
            : index - 1U;
    } else {
        index = (index + 1U) % fields.size();
    }
    active_field = fields[index];
}

void LoginFormState::append_ascii(char value) {
    const auto byte = static_cast<unsigned char>(value);
    if (byte < 32U || byte > 126U) {
        return;
    }

    auto& target = active_value();
    if (target.size() >= active_limit()) {
        return;
    }
    target.push_back(value);
    error.clear();
}

void LoginFormState::backspace() {
    auto& target = active_value();
    if (!target.empty()) {
        target.pop_back();
    }
    error.clear();
}

bool LoginFormState::complete() const noexcept {
    return !server.empty() &&
           !username.empty() &&
           !password.empty() &&
           !region.empty();
}

LoginRequest LoginFormState::request() const {
    if (!complete()) {
        throw std::runtime_error(
            "Server, username, password and Region are required");
    }

    return {
        .core_base_url = server,
        .username = username,
        .password = password,
        .region_id = region,
        .spawn_x = 128.0,
        .spawn_y = 128.0,
        .spawn_z = 25.0,
    };
}

std::string LoginFormState::masked_password() const {
    return std::string(password.size(), '*');
}

void LoginFormState::clear_password() noexcept {
    for (auto& value : password) {
        value = '\0';
    }
    password.clear();
}

std::string& LoginFormState::active_value() noexcept {
    switch (active_field) {
    case LoginField::server:
        return server;
    case LoginField::username:
        return username;
    case LoginField::password:
        return password;
    case LoginField::region:
        return region;
    }

    return server;
}

const std::string& LoginFormState::active_value() const noexcept {
    switch (active_field) {
    case LoginField::server:
        return server;
    case LoginField::username:
        return username;
    case LoginField::password:
        return password;
    case LoginField::region:
        return region;
    }

    return server;
}

std::size_t LoginFormState::active_limit() const noexcept {
    switch (active_field) {
    case LoginField::server:
        return 512U;
    case LoginField::username:
        return 128U;
    case LoginField::password:
        return 512U;
    case LoginField::region:
        return 256U;
    }

    return 128U;
}

} // namespace ogl::viewer::app
