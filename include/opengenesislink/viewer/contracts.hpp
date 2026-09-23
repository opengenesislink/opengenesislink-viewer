#pragma once

#include <string_view>

namespace ogl::viewer::contracts {

inline constexpr std::string_view release = "ogl-release-v1";
inline constexpr std::string_view viewer_bootstrap = "ogl-viewer-bootstrap-v1";
inline constexpr std::string_view scene = "scene-v2";
inline constexpr int api_version = 1;
inline constexpr int scene_transport_protocol = 1;

} // namespace ogl::viewer::contracts
