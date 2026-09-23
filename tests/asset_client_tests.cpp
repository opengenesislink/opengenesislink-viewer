#include "opengenesislink/viewer/core/asset_client.hpp"
#include "opengenesislink/viewer/core/http_transport.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace ogl::viewer::core;

class FakeTransport final : public HttpTransport {
public:
    std::vector<HttpResponse> responses;
    std::vector<HttpRequest> requests;

    HttpResponse perform(
        const HttpRequest& request) override {
        requests.push_back(request);
        if (responses.empty()) {
            throw std::runtime_error(
                "FakeTransport has no response");
        }
        auto response =
            std::move(responses.front());
        responses.erase(responses.begin());
        return response;
    }
};

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

void test_asset_fetch() {
    FakeTransport transport;
    transport.responses.push_back({
        200,
        R"({
            "asset":{
                "id":"abc123",
                "name":"Binary",
                "mime_type":"application/octet-stream",
                "size":5,
                "permissions":31,
                "next_owner_permissions":6,
                "created_unix":123,
                "content_hash":"deadbeef"
            },
            "data_base64":"AAEC/f4="
        })"
    });

    AssetClient client(transport);
    const auto asset = client.fetch(
        "https://core.example/",
        "bearer-token",
        "abc123");

    require(transport.requests.size() == 1U,
            "Asset fetch request count mismatch");
    require(transport.requests[0].method == "GET",
            "Asset fetch must use GET");
    require(transport.requests[0].url ==
                "https://core.example/v1/assets/abc123",
            "Asset fetch URL mismatch");
    require(
        transport.requests[0].headers.at("Authorization") ==
            "Bearer bearer-token",
        "Asset fetch bearer header mismatch");

    require(asset.metadata.id == "abc123",
            "Asset id mismatch");
    require(asset.metadata.content_hash == "deadbeef",
            "Asset content hash mismatch");
    require(asset.data.size() == 5U,
            "Asset decoded size mismatch");
    require(
        static_cast<unsigned char>(asset.data[0]) == 0U &&
        static_cast<unsigned char>(asset.data[1]) == 1U &&
        static_cast<unsigned char>(asset.data[2]) == 2U &&
        static_cast<unsigned char>(asset.data[3]) == 253U &&
        static_cast<unsigned char>(asset.data[4]) == 254U,
        "Asset decoded bytes mismatch");
}

void test_asset_response_validation() {
    FakeTransport wrong_id;
    wrong_id.responses.push_back({
        200,
        R"({
            "asset":{"id":"other","size":1},
            "data_base64":"AA=="
        })"
    });
    AssetClient wrong_id_client(wrong_id);
    require_throws(
        [&] {
            (void)wrong_id_client.fetch(
                "https://core.example",
                "token",
                "expected");
        },
        "Asset id mismatch must fail");

    FakeTransport wrong_size;
    wrong_size.responses.push_back({
        200,
        R"({
            "asset":{"id":"abc","size":2},
            "data_base64":"AA=="
        })"
    });
    AssetClient wrong_size_client(wrong_size);
    require_throws(
        [&] {
            (void)wrong_size_client.fetch(
                "https://core.example",
                "token",
                "abc");
        },
        "Asset size mismatch must fail");

    FakeTransport invalid_base64;
    invalid_base64.responses.push_back({
        200,
        R"({
            "asset":{"id":"abc","size":1},
            "data_base64":"!!!!"
        })"
    });
    AssetClient invalid_base64_client(
        invalid_base64);
    require_throws(
        [&] {
            (void)invalid_base64_client.fetch(
                "https://core.example",
                "token",
                "abc");
        },
        "Invalid Asset base64 must fail");
}

void test_asset_decoded_limit() {
    FakeTransport transport;
    transport.responses.push_back({
        200,
        R"({
            "asset":{"id":"abc","size":3},
            "data_base64":"AQID"
        })"
    });
    AssetClient client(transport, 2U);
    require_throws(
        [&] {
            (void)client.fetch(
                "https://core.example",
                "token",
                "abc");
        },
        "Asset byte limit must be enforced");
}

} // namespace

int main() {
    try {
        test_asset_fetch();
        test_asset_response_validation();
        test_asset_decoded_limit();
        std::cout
            << "OpenGenesisLINK Viewer Asset client tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr
            << "Asset client test failure: "
            << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}
