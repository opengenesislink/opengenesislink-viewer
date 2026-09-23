#pragma once

#include "opengenesislink/viewer/core/bootstrap_content.hpp"
#include "opengenesislink/viewer/core/http_transport.hpp"

#include <cstddef>
#include <string>

namespace ogl::viewer::core {

struct AssetBlob {
    AssetMetadata metadata;
    std::string data;
};

class AssetClient {
public:
    explicit AssetClient(
        HttpTransport& transport,
        std::size_t max_decoded_bytes =
            16U * 1024U * 1024U);

    [[nodiscard]] AssetBlob fetch(
        const std::string& core_base_url,
        const std::string& bearer_token,
        const std::string& asset_id) const;

private:
    HttpTransport& transport_;
    std::size_t max_decoded_bytes_;
};

} // namespace ogl::viewer::core
