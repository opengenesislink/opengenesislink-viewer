#pragma once

#include "opengenesislink/viewer/core/http_transport.hpp"

#include <chrono>

namespace ogl::viewer::core {

class CurlHttpTransport final : public HttpTransport {
public:
    explicit CurlHttpTransport(
        std::chrono::milliseconds connect_timeout = std::chrono::seconds(5),
        std::chrono::milliseconds request_timeout = std::chrono::seconds(15));

    HttpResponse perform(const HttpRequest& request) override;

private:
    std::chrono::milliseconds connect_timeout_;
    std::chrono::milliseconds request_timeout_;
};

} // namespace ogl::viewer::core
