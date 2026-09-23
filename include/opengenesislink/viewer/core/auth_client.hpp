#pragma once

#include "opengenesislink/viewer/core/http_transport.hpp"

#include <cstdint>
#include <string>

namespace ogl::viewer::core {

struct AuthenticatedUser {
    std::string id;
    std::string username;
    std::string display_name;
};

struct LoginResult {
    AuthenticatedUser user;
    std::string token;
    std::int64_t expires_unix = 0;
};

class AuthClient {
public:
    explicit AuthClient(HttpTransport& transport);

    LoginResult login(
        const std::string& core_base_url,
        const std::string& username,
        const std::string& password) const;

private:
    HttpTransport& transport_;
};

} // namespace ogl::viewer::core
