#include "opengenesislink/viewer/app/login_form.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using ogl::viewer::app::LoginField;
using ogl::viewer::app::LoginFormState;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <typename Function>
void require_throws(Function&& function, const char* message) {
    bool threw = false;
    try {
        function();
    } catch (const std::exception&) {
        threw = true;
    }
    require(threw, message);
}

void type(LoginFormState& form, const std::string& value) {
    for (const char character : value) {
        form.append_ascii(character);
    }
}

void test_focus_cycle() {
    LoginFormState form;
    require(form.active_field == LoginField::server,
            "initial login focus must be server");

    form.focus_next();
    require(form.active_field == LoginField::username,
            "focus did not advance to username");
    form.focus_next();
    require(form.active_field == LoginField::password,
            "focus did not advance to password");
    form.focus_next();
    require(form.active_field == LoginField::region,
            "focus did not advance to Region");
    form.focus_next();
    require(form.active_field == LoginField::server,
            "focus did not wrap to server");

    form.focus_next(true);
    require(form.active_field == LoginField::region,
            "reverse focus did not wrap to Region");
}

void test_editing_and_masking() {
    LoginFormState form;
    form.focus(LoginField::server);
    type(form, "https://grid.example");
    require(form.server == "https://grid.example",
            "server field editing mismatch");

    form.focus(LoginField::password);
    type(form, "Secret42!");
    require(form.password == "Secret42!",
            "password field editing mismatch");
    require(form.masked_password() == "*********",
            "password mask length mismatch");

    form.backspace();
    require(form.password == "Secret42",
            "password backspace mismatch");

    form.append_ascii('\n');
    require(form.password == "Secret42",
            "non-printable character must be ignored");
}

void test_request_validation_and_defaults() {
    LoginFormState form;
    require(!form.complete(),
            "empty login form must be incomplete");

    require_throws(
        [&] { (void)form.request(); },
        "incomplete login form must not build request");

    form.server = "https://core.example";
    form.username = "resident";
    form.password = "secret";
    form.region = "region-1";

    require(form.complete(),
            "complete login form was not recognized");

    const auto request = form.request();
    require(request.core_base_url == "https://core.example",
            "login request server mismatch");
    require(request.username == "resident",
            "login request username mismatch");
    require(request.password == "secret",
            "login request password mismatch");
    require(request.region_id == "region-1",
            "login request Region mismatch");
    require(request.spawn_x == 128.0 &&
            request.spawn_y == 128.0 &&
            request.spawn_z == 25.0,
            "login request default spawn mismatch");
}

void test_password_clear_and_field_limits() {
    LoginFormState form;
    form.password = "sensitive";
    form.clear_password();
    require(form.password.empty(),
            "password clear did not empty the field");

    form.focus(LoginField::username);
    for (std::size_t index = 0U; index < 200U; ++index) {
        form.append_ascii('x');
    }
    require(form.username.size() == 128U,
            "username field exceeded configured limit");
}

} // namespace

int main() {
    try {
        test_focus_cycle();
        test_editing_and_masking();
        test_request_validation_and_defaults();
        test_password_clear_and_field_limits();
        std::cout
            << "OpenGenesisLINK Viewer login form tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr
            << "Login form test failure: "
            << ex.what()
            << "\n";
        return EXIT_FAILURE;
    }
}
