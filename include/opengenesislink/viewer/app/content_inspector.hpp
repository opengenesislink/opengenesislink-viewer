#pragma once

#include "opengenesislink/viewer/core/bootstrap_content.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace ogl::viewer::app {

enum class ContentInspectorSection {
    appearance,
    inventory,
    assets
};

struct ContentInspectorView {
    ContentInspectorSection section =
        ContentInspectorSection::appearance;
    std::string title;
    std::vector<std::string> lines;
    std::size_t page = 0U;
    std::size_t page_count = 1U;
};

[[nodiscard]] ContentInspectorSection
next_content_inspector_section(
    ContentInspectorSection section) noexcept;

[[nodiscard]] ContentInspectorView
build_content_inspector_view(
    const core::BootstrapContent& content,
    ContentInspectorSection section,
    std::size_t requested_page,
    std::size_t page_size = 12U);

} // namespace ogl::viewer::app
