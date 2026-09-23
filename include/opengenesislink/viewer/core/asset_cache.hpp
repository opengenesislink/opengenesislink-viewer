#pragma once

#include "opengenesislink/viewer/core/asset_client.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ogl::viewer::core {

class AssetCache {
public:
    AssetCache(
        std::size_t max_entries = 128U,
        std::size_t max_bytes =
            64U * 1024U * 1024U);

    [[nodiscard]] const AssetBlob* find(
        const AssetMetadata& expected);

    [[nodiscard]] const AssetBlob* find_by_id(
        std::string_view asset_id);

    [[nodiscard]] bool put(AssetBlob asset);

    void clear() noexcept;

    [[nodiscard]] std::size_t entries() const noexcept;
    [[nodiscard]] std::size_t bytes() const noexcept;
    [[nodiscard]] std::size_t max_entries() const noexcept;
    [[nodiscard]] std::size_t max_bytes() const noexcept;

private:
    struct Entry {
        AssetBlob asset;
        std::uint64_t access = 0;
    };

    void evict_until_within_limits(
        std::string_view protected_id = {});

    std::size_t max_entries_;
    std::size_t max_bytes_;
    std::size_t bytes_ = 0U;
    std::uint64_t access_counter_ = 0U;
    std::unordered_map<std::string, Entry> entries_;
};

} // namespace ogl::viewer::core
