#include "opengenesislink/viewer/core/curl_http_transport.hpp"

#include <curl/curl.h>

#include <memory>
#include <stdexcept>
#include <string>

namespace ogl::viewer::core {
namespace {

struct CurlGlobal {
    CurlGlobal() {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
            throw std::runtime_error("curl_global_init failed");
        }
    }
    ~CurlGlobal() {
        curl_global_cleanup();
    }
};

CurlGlobal& curl_global() {
    static CurlGlobal global;
    return global;
}

std::size_t write_body(char* data, std::size_t size, std::size_t count, void* userdata) {
    const auto bytes = size * count;
    auto* body = static_cast<std::string*>(userdata);
    body->append(data, bytes);
    return bytes;
}

struct HeaderListDeleter {
    void operator()(curl_slist* value) const noexcept {
        curl_slist_free_all(value);
    }
};

} // namespace

CurlHttpTransport::CurlHttpTransport(
    std::chrono::milliseconds connect_timeout,
    std::chrono::milliseconds request_timeout)
    : connect_timeout_(connect_timeout),
      request_timeout_(request_timeout) {
    (void)curl_global();
}

HttpResponse CurlHttpTransport::perform(const HttpRequest& request) {
    CURL* raw = curl_easy_init();
    if (raw == nullptr) {
        throw std::runtime_error("curl_easy_init failed");
    }

    const std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> handle(raw, &curl_easy_cleanup);

    curl_slist* headers_raw = nullptr;
    for (const auto& [name, value] : request.headers) {
        const std::string header = name + ": " + value;
        auto* next = curl_slist_append(headers_raw, header.c_str());
        if (next == nullptr) {
            curl_slist_free_all(headers_raw);
            throw std::runtime_error("curl_slist_append failed");
        }
        headers_raw = next;
    }
    const std::unique_ptr<curl_slist, HeaderListDeleter> headers(headers_raw);

    std::string response_body;
    char error_buffer[CURL_ERROR_SIZE] = {};

    curl_easy_setopt(handle.get(), CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(handle.get(), CURLOPT_CUSTOMREQUEST, request.method.c_str());
    curl_easy_setopt(handle.get(), CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(handle.get(), CURLOPT_WRITEFUNCTION, &write_body);
    curl_easy_setopt(handle.get(), CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(handle.get(), CURLOPT_ERRORBUFFER, error_buffer);
    curl_easy_setopt(handle.get(), CURLOPT_CONNECTTIMEOUT_MS,
                     static_cast<long>(connect_timeout_.count()));
    curl_easy_setopt(handle.get(), CURLOPT_TIMEOUT_MS,
                     static_cast<long>(request_timeout_.count()));
    curl_easy_setopt(handle.get(), CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(handle.get(), CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle.get(), CURLOPT_USERAGENT,
                     "OpenGenesisLINK-Viewer/" OGL_VIEWER_VERSION);

    if (!request.body.empty()) {
        curl_easy_setopt(handle.get(), CURLOPT_POSTFIELDS, request.body.c_str());
        curl_easy_setopt(handle.get(), CURLOPT_POSTFIELDSIZE,
                         static_cast<long>(request.body.size()));
    }

    const CURLcode code = curl_easy_perform(handle.get());
    if (code != CURLE_OK) {
        const std::string detail =
            error_buffer[0] != '\0' ? error_buffer : curl_easy_strerror(code);
        throw std::runtime_error("HTTP transport failed: " + detail);
    }

    long status = 0;
    curl_easy_getinfo(handle.get(), CURLINFO_RESPONSE_CODE, &status);

    return {
        .status = static_cast<int>(status),
        .body = std::move(response_body),
    };
}

} // namespace ogl::viewer::core
