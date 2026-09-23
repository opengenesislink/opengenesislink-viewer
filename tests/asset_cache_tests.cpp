#include "opengenesislink/viewer/core/asset_cache.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using namespace ogl::viewer::core;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

AssetBlob asset(
    std::string id,
    std::string hash,
    std::string data) {
    AssetBlob result;
    result.metadata.id = std::move(id);
    result.metadata.content_hash =
        std::move(hash);
    result.metadata.size =
        static_cast<std::uint64_t>(
            data.size());
    result.data = std::move(data);
    return result;
}

void test_hash_invalidation() {
    AssetCache cache(4U, 64U);
    require(
        cache.put(asset("a", "hash-1", "1234")),
        "Asset cache put failed");

    AssetMetadata expected;
    expected.id = "a";
    expected.content_hash = "hash-1";
    expected.size = 4U;
    require(cache.find(expected) != nullptr,
            "matching cached Asset not found");

    expected.content_hash = "hash-2";
    require(cache.find(expected) == nullptr,
            "changed content hash did not invalidate cache");
    require(cache.entries() == 0U &&
            cache.bytes() == 0U,
            "invalidated Asset remained accounted");
}

void test_lru_entry_eviction() {
    AssetCache cache(2U, 64U);
    (void)cache.put(asset("a", "1", "aaaa"));
    (void)cache.put(asset("b", "2", "bbbb"));

    require(cache.find_by_id("a") != nullptr,
            "Asset a cache touch failed");

    (void)cache.put(asset("c", "3", "cccc"));

    require(cache.find_by_id("a") != nullptr,
            "recent Asset a was evicted");
    require(cache.find_by_id("b") == nullptr,
            "least recently used Asset b was not evicted");
    require(cache.find_by_id("c") != nullptr,
            "new Asset c was not retained");
}

void test_byte_budget_and_oversize_rejection() {
    AssetCache cache(10U, 8U);
    (void)cache.put(asset("a", "1", "123456"));
    (void)cache.put(asset("b", "2", "abcdef"));

    require(cache.bytes() <= 8U,
            "Asset cache exceeded byte budget");
    require(cache.entries() == 1U,
            "Asset byte budget did not evict old entry");
    require(cache.find_by_id("b") != nullptr,
            "new Asset was not protected during eviction");

    require(
        !cache.put(asset(
            "huge",
            "3",
            "123456789")),
        "oversized Asset should not enter cache");
    require(cache.find_by_id("huge") == nullptr,
            "oversized Asset remained cached");
}

} // namespace

int main() {
    try {
        test_hash_invalidation();
        test_lru_entry_eviction();
        test_byte_budget_and_oversize_rejection();
        std::cout
            << "OpenGenesisLINK Viewer Asset cache tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr
            << "Asset cache test failure: "
            << ex.what() << "\n";
        return EXIT_FAILURE;
    }
}
