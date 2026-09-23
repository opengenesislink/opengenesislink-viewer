#include "opengenesislink/viewer/core/asset_cache.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ogl::viewer::core {
namespace {

bool metadata_matches(
    const AssetMetadata& cached,
    const AssetMetadata& expected) {
    if (cached.id != expected.id) {
        return false;
    }
    if (!expected.content_hash.empty() &&
        cached.content_hash !=
            expected.content_hash) {
        return false;
    }
    if (expected.size != 0U &&
        cached.size != expected.size) {
        return false;
    }
    return true;
}

} // namespace

AssetCache::AssetCache(
    std::size_t max_entries,
    std::size_t max_bytes)
    : max_entries_(max_entries),
      max_bytes_(max_bytes) {
    if (max_entries_ == 0U ||
        max_bytes_ == 0U) {
        throw std::invalid_argument(
            "Asset cache limits must be non-zero");
    }
}

const AssetBlob* AssetCache::find(
    const AssetMetadata& expected) {
    const auto found =
        entries_.find(expected.id);
    if (found == entries_.end()) {
        return nullptr;
    }
    if (!metadata_matches(
            found->second.asset.metadata,
            expected)) {
        bytes_ -=
            found->second.asset.data.size();
        entries_.erase(found);
        return nullptr;
    }

    found->second.access =
        ++access_counter_;
    return &found->second.asset;
}

const AssetBlob* AssetCache::find_by_id(
    std::string_view asset_id) {
    const auto found =
        entries_.find(std::string(asset_id));
    if (found == entries_.end()) {
        return nullptr;
    }
    found->second.access =
        ++access_counter_;
    return &found->second.asset;
}

bool AssetCache::put(AssetBlob asset) {
    if (asset.metadata.id.empty()) {
        throw std::invalid_argument(
            "Asset cache requires a non-empty asset id");
    }
    if (asset.metadata.size !=
        static_cast<std::uint64_t>(
            asset.data.size())) {
        throw std::invalid_argument(
            "Asset cache data size does not match metadata");
    }
    if (asset.data.size() > max_bytes_) {
        return false;
    }

    if (const auto existing =
            entries_.find(asset.metadata.id);
        existing != entries_.end()) {
        bytes_ -=
            existing->second.asset.data.size();
        entries_.erase(existing);
    }

    const auto id = asset.metadata.id;
    bytes_ += asset.data.size();
    entries_.emplace(
        id,
        Entry{
            .asset = std::move(asset),
            .access = ++access_counter_,
        });

    evict_until_within_limits(id);
    return entries_.contains(id);
}

void AssetCache::evict_until_within_limits(
    std::string_view protected_id) {
    while (entries_.size() > max_entries_ ||
           bytes_ > max_bytes_) {
        auto candidate = entries_.end();
        for (auto it = entries_.begin();
             it != entries_.end();
             ++it) {
            if (it->first == protected_id) {
                continue;
            }
            if (candidate == entries_.end() ||
                it->second.access <
                    candidate->second.access) {
                candidate = it;
            }
        }

        if (candidate == entries_.end()) {
            break;
        }

        bytes_ -=
            candidate->second.asset.data.size();
        entries_.erase(candidate);
    }
}

void AssetCache::clear() noexcept {
    entries_.clear();
    bytes_ = 0U;
    access_counter_ = 0U;
}

std::size_t AssetCache::entries() const noexcept {
    return entries_.size();
}

std::size_t AssetCache::bytes() const noexcept {
    return bytes_;
}

std::size_t AssetCache::max_entries() const noexcept {
    return max_entries_;
}

std::size_t AssetCache::max_bytes() const noexcept {
    return max_bytes_;
}

} // namespace ogl::viewer::core
