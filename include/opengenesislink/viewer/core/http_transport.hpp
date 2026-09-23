#pragma once

#include <map>
#include <string>

namespace ogl::viewer::core {

struct HttpRequest {
    std::string method;
    std::string url;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int status = 0;
    std::string body;
};

class HttpTransport {
public:
    virtual ~HttpTransport() = default;
    virtual HttpResponse perform(const HttpRequest& request) = 0;
};

} // namespace ogl::viewer::core
