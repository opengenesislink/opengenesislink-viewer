#include "opengenesislink/viewer/core/asset_client.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <cctype>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace ogl::viewer::core {
namespace {

std::string endpoint(
    const std::string& base,
    const std::string& asset_id) {
    if (base.empty()) {
        throw std::invalid_argument(
            "Core base URL must not be empty");
    }
    if (asset_id.empty()) {
        throw std::invalid_argument(
            "Asset id must not be empty");
    }
    for (const unsigned char value : asset_id) {
        if (!std::isalnum(value) &&
            value != '-' &&
            value != '_' &&
            value != '.') {
            throw std::invalid_argument(
                "Asset id contains an unsafe URL character");
        }
    }

    const auto prefix =
        base.back() == '/'
            ? base.substr(0, base.size() - 1)
            : base;
    return prefix + "/v1/assets/" + asset_id;
}

std::uint64_t json_u64(
    const nlohmann::json& object,
    const char* key) {
    const auto found = object.find(key);
    if (found == object.end()) {
        return 0U;
    }
    if (found->is_number_unsigned()) {
        return found->get<std::uint64_t>();
    }
    if (found->is_number_integer()) {
        const auto value =
            found->get<std::int64_t>();
        return value >= 0
            ? static_cast<std::uint64_t>(value)
            : 0U;
    }
    return 0U;
}

std::uint32_t json_u32(
    const nlohmann::json& object,
    const char* key) {
    const auto value = json_u64(object, key);
    if (value >
        static_cast<std::uint64_t>(
            std::numeric_limits<std::uint32_t>::max())) {
        throw std::runtime_error(
            std::string{"Asset field is out of range: "} +
            key);
    }
    return static_cast<std::uint32_t>(value);
}

std::int64_t json_i64(
    const nlohmann::json& object,
    const char* key) {
    const auto found = object.find(key);
    return found != object.end() &&
           found->is_number_integer()
        ? found->get<std::int64_t>()
        : 0;
}

std::string json_string(
    const nlohmann::json& object,
    const char* key) {
    const auto found = object.find(key);
    return found != object.end() &&
           found->is_string()
        ? found->get<std::string>()
        : std::string{};
}

int base64_value(char character) {
    if (character >= 'A' && character <= 'Z') {
        return character - 'A';
    }
    if (character >= 'a' && character <= 'z') {
        return character - 'a' + 26;
    }
    if (character >= '0' && character <= '9') {
        return character - '0' + 52;
    }
    if (character == '+') {
        return 62;
    }
    if (character == '/') {
        return 63;
    }
    return -1;
}

std::string decode_base64(
    std::string_view encoded,
    std::size_t max_decoded_bytes) {
    if (encoded.empty()) {
        return {};
    }
    if ((encoded.size() % 4U) != 0U) {
        throw std::runtime_error(
            "Asset data_base64 has invalid length");
    }

    std::size_t padding = 0U;
    if (encoded.ends_with("==")) {
        padding = 2U;
    } else if (encoded.ends_with("=")) {
        padding = 1U;
    }

    const auto groups = encoded.size() / 4U;
    if (groups >
        (std::numeric_limits<std::size_t>::max() / 3U)) {
        throw std::runtime_error(
            "Asset data_base64 size overflows");
    }
    const auto decoded_size =
        groups * 3U - padding;
    if (decoded_size > max_decoded_bytes) {
        throw std::runtime_error(
            "Asset exceeds Viewer decoded byte limit");
    }

    std::string output;
    output.reserve(decoded_size);

    for (std::size_t offset = 0U;
         offset < encoded.size();
         offset += 4U) {
        const bool last =
            offset + 4U == encoded.size();
        const char c0 = encoded[offset];
        const char c1 = encoded[offset + 1U];
        const char c2 = encoded[offset + 2U];
        const char c3 = encoded[offset + 3U];

        const int v0 = base64_value(c0);
        const int v1 = base64_value(c1);
        const int v2 =
            c2 == '=' ? 0 : base64_value(c2);
        const int v3 =
            c3 == '=' ? 0 : base64_value(c3);

        if (v0 < 0 || v1 < 0 ||
            v2 < 0 || v3 < 0) {
            throw std::runtime_error(
                "Asset data_base64 contains invalid characters");
        }
        if ((c2 == '=' && c3 != '=') ||
            (!last && (c2 == '=' || c3 == '='))) {
            throw std::runtime_error(
                "Asset data_base64 has invalid padding");
        }

        const auto value =
            (static_cast<std::uint32_t>(v0) << 18U) |
            (static_cast<std::uint32_t>(v1) << 12U) |
            (static_cast<std::uint32_t>(v2) << 6U) |
            static_cast<std::uint32_t>(v3);

        output.push_back(
            static_cast<char>((value >> 16U) & 0xFFU));
        if (c2 != '=') {
            output.push_back(
                static_cast<char>((value >> 8U) & 0xFFU));
        }
        if (c3 != '=') {
            output.push_back(
                static_cast<char>(value & 0xFFU));
        }
    }

    if (output.size() != decoded_size) {
        throw std::runtime_error(
            "Asset data_base64 decoded size mismatch");
    }
    return output;
}

AssetMetadata parse_metadata(
    const nlohmann::json& asset) {
    if (!asset.is_object()) {
        throw std::runtime_error(
            "Asset response metadata must be an object");
    }

    AssetMetadata result{
        .id = json_string(asset, "id"),
        .name = json_string(asset, "name"),
        .mime_type =
            json_string(asset, "mime_type"),
        .content_hash =
            json_string(asset, "content_hash"),
        .size = json_u64(asset, "size"),
        .permissions =
            json_u32(asset, "permissions"),
        .next_owner_permissions =
            json_u32(
                asset,
                "next_owner_permissions"),
        .created_unix =
            json_i64(asset, "created_unix"),
    };

    if (result.id.empty()) {
        throw std::runtime_error(
            "Asset response is missing asset id");
    }
    return result;
}

} // namespace

AssetClient::AssetClient(
    HttpTransport& transport,
    std::size_t max_decoded_bytes)
    : transport_(transport),
      max_decoded_bytes_(max_decoded_bytes) {
    if (max_decoded_bytes_ == 0U) {
        throw std::invalid_argument(
            "Asset byte limit must be non-zero");
    }
}

AssetBlob AssetClient::fetch(
    const std::string& core_base_url,
    const std::string& bearer_token,
    const std::string& asset_id) const {
    if (bearer_token.empty()) {
        throw std::invalid_argument(
            "Bearer token must not be empty");
    }

    const auto response = transport_.perform({
        .method = "GET",
        .url = endpoint(
            core_base_url,
            asset_id),
        .headers = {
            {
                "Authorization",
                "Bearer " + bearer_token,
            },
        },
        .body = {},
        .max_response_bytes =
            max_decoded_bytes_ * 2U +
            1024U * 1024U,
    });

    if (response.status < 200 ||
        response.status >= 300) {
        throw std::runtime_error(
            "Asset fetch failed with HTTP " +
            std::to_string(response.status));
    }

    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(
            response.body);
    } catch (const nlohmann::json::exception& ex) {
        throw std::runtime_error(
            std::string{
                "Asset fetch returned invalid JSON: "} +
            ex.what());
    }

    const auto asset = doc.find("asset");
    const auto encoded =
        doc.find("data_base64");
    if (asset == doc.end() ||
        encoded == doc.end() ||
        !encoded->is_string()) {
        throw std::runtime_error(
            "Asset fetch response is incomplete");
    }

    auto metadata = parse_metadata(*asset);
    if (metadata.id != asset_id) {
        throw std::runtime_error(
            "Asset fetch returned a different asset id");
    }

    auto data = decode_base64(
        encoded->get_ref<const std::string&>(),
        max_decoded_bytes_);

    if (metadata.size !=
        static_cast<std::uint64_t>(data.size())) {
        throw std::runtime_error(
            "Asset decoded size does not match metadata");
    }

    return {
        .metadata = std::move(metadata),
        .data = std::move(data),
    };
}

} // namespace ogl::viewer::core
