#include "opengenesislink/viewer/core/auth_client.hpp"

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <string_view>

namespace ogl::viewer::core {
namespace {

std::string endpoint(const std::string& base, std::string_view path) {
    if (base.empty()) {
        throw std::invalid_argument("Core base URL must not be empty");
    }
    return (base.back() == '/')
        ? base.substr(0, base.size() - 1) + std::string(path)
        : base + std::string(path);
}

} // namespace

AuthClient::AuthClient(HttpTransport& transport)
    : transport_(transport) {}

LoginResult AuthClient::login(
    const std::string& core_base_url,
    const std::string& username,
    const std::string& password) const {

    if (username.empty() || password.empty()) {
        throw std::invalid_argument("Username and password must not be empty");
    }

    const nlohmann::json payload = {
        {"username", username},
        {"password", password},
    };

    const auto response = transport_.perform({
        .method = "POST",
        .url = endpoint(core_base_url, "/v1/auth/login"),
        .headers = {{"Content-Type", "application/json"}},
        .body = payload.dump(),
    });

    if (response.status < 200 || response.status >= 300) {
        throw std::runtime_error(
            "Login failed with HTTP " + std::to_string(response.status));
    }

    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(response.body);
    } catch (const nlohmann::json::exception& ex) {
        throw std::runtime_error(std::string("Login returned invalid JSON: ") + ex.what());
    }

    LoginResult result;
    result.token = doc.value("token", "");
    result.expires_unix = doc.value("expires_unix", std::int64_t{0});

    if (const auto user = doc.find("user"); user != doc.end() && user->is_object()) {
        result.user.id = user->value("id", "");
        result.user.username = user->value("username", "");
        result.user.display_name = user->value("display_name", "");
    }

    if (result.token.empty()) {
        throw std::runtime_error("Login response is missing bearer token");
    }

    return result;
}

} // namespace ogl::viewer::core
