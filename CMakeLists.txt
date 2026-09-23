cmake_minimum_required(VERSION 3.25)
project(OpenGenesisLINKViewer VERSION 0.8.0 LANGUAGES CXX)

include(GNUInstallDirs)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" OGL_VIEWER_VERSION_LABEL)
string(STRIP "${OGL_VIEWER_VERSION_LABEL}" OGL_VIEWER_VERSION_LABEL)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OGL_VIEWER_BUILD_TESTS "Build OpenGenesisLINK Viewer tests" ON)
option(OGL_VIEWER_ENABLE_WERROR "Treat warnings as errors" OFF)

find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm CONFIG REQUIRED)

add_library(opengenesis-viewer-core
    src/core/curl_http_transport.cpp
    src/core/release_discovery.cpp
    src/core/auth_client.cpp
    src/core/viewer_bootstrap.cpp
    src/scene/frame.cpp
    src/scene/session_protocol.cpp
    src/scene/transport.cpp
    src/scene/scene_session.cpp
    src/scene/terrain_protocol.cpp
    src/world/world_model.cpp
    src/world/render_world.cpp
    src/world/terrain_cache.cpp
    src/world/terrain_patch.cpp
    src/world/scene_synchronizer.cpp
    src/input/camera.cpp
    src/app/login_flow.cpp
    src/app/viewer_runtime.cpp
    src/app/launch_options.cpp
    src/app/login_form.cpp
)
add_library(OpenGenesis::ViewerCore ALIAS opengenesis-viewer-core)

target_include_directories(opengenesis-viewer-core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(opengenesis-viewer-core PUBLIC
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(opengenesis-viewer-core PRIVATE ws2_32)
endif()

target_compile_definitions(opengenesis-viewer-core PUBLIC
    OGL_VIEWER_VERSION="${OGL_VIEWER_VERSION_LABEL}"
)

if(MSVC)
    target_compile_options(opengenesis-viewer-core PRIVATE /W4 /permissive- /Zc:__cplusplus)
    if(OGL_VIEWER_ENABLE_WERROR)
        target_compile_options(opengenesis-viewer-core PRIVATE /WX)
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(opengenesis-viewer-core PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )
    if(OGL_VIEWER_ENABLE_WERROR)
        target_compile_options(opengenesis-viewer-core PRIVATE -Werror)
    endif()
endif()

add_library(opengenesis-viewer-graphics
    src/render/opengl_renderer.cpp
    src/render/ui_renderer.cpp
    src/app/desktop_application.cpp
)
add_library(OpenGenesis::ViewerGraphics ALIAS opengenesis-viewer-graphics)

target_include_directories(opengenesis-viewer-graphics PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(opengenesis-viewer-graphics PUBLIC
    OpenGenesis::ViewerCore
    OpenGL::GL
    glfw
    GLEW::GLEW
    glm::glm
)

if(MSVC)
    target_compile_options(opengenesis-viewer-graphics PRIVATE /W4 /permissive- /Zc:__cplusplus)
    if(OGL_VIEWER_ENABLE_WERROR)
        target_compile_options(opengenesis-viewer-graphics PRIVATE /WX)
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(opengenesis-viewer-graphics PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )
    if(OGL_VIEWER_ENABLE_WERROR)
        target_compile_options(opengenesis-viewer-graphics PRIVATE -Werror)
    endif()
endif()

add_executable(ogl-viewer apps/viewer/main.cpp)
target_link_libraries(ogl-viewer PRIVATE
    OpenGenesis::ViewerCore
    OpenGenesis::ViewerGraphics
)

if(WIN32)
    set(OGL_VIEWER_ICON_ICO
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/branding/opengenesislink-viewer.ico")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
    configure_file(
        resources/windows/opengenesislink-viewer.rc.in
        "${CMAKE_CURRENT_BINARY_DIR}/generated/opengenesislink-viewer.rc"
        @ONLY
    )
    target_sources(
        ogl-viewer PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/opengenesislink-viewer.rc"
    )
endif()

add_executable(ogl-viewer-contract-smoke apps/contract-smoke/main.cpp)
target_link_libraries(ogl-viewer-contract-smoke PRIVATE OpenGenesis::ViewerCore)

install(TARGETS opengenesis-viewer-core ogl-viewer ogl-viewer-contract-smoke
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY include/ DESTINATION include)

if(UNIX AND NOT APPLE)
    install(
        FILES assets/branding/opengenesislink-viewer.png
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/64x64/apps"
        RENAME opengenesislink-viewer.png
    )
    install(
        FILES packaging/linux/opengenesislink-viewer.desktop
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
    )
endif()

include(CTest)
if(BUILD_TESTING AND OGL_VIEWER_BUILD_TESTS)
    add_test(NAME viewer-app-version COMMAND ogl-viewer --version)

    add_executable(ogl-viewer-core-tests tests/core_contract_tests.cpp)
    target_link_libraries(ogl-viewer-core-tests PRIVATE OpenGenesis::ViewerCore)
    add_test(NAME viewer-core-contracts COMMAND ogl-viewer-core-tests)

    add_executable(ogl-viewer-scene-tests tests/scene_protocol_tests.cpp)
    target_link_libraries(ogl-viewer-scene-tests PRIVATE OpenGenesis::ViewerCore)
    add_test(NAME viewer-scene-protocol COMMAND ogl-viewer-scene-tests)

    add_executable(ogl-viewer-scene-session-tests tests/scene_session_tests.cpp)
    target_link_libraries(ogl-viewer-scene-session-tests PRIVATE OpenGenesis::ViewerCore)
    add_test(NAME viewer-scene-session COMMAND ogl-viewer-scene-session-tests)

    add_executable(ogl-viewer-world-model-tests tests/world_model_tests.cpp)
    target_link_libraries(ogl-viewer-world-model-tests PRIVATE OpenGenesis::ViewerCore)
    add_test(NAME viewer-world-model COMMAND ogl-viewer-world-model-tests)

    add_executable(ogl-viewer-render-world-tests tests/render_world_foundation_tests.cpp)
    target_link_libraries(ogl-viewer-render-world-tests PRIVATE OpenGenesis::ViewerCore)
    add_test(NAME viewer-render-world-foundation COMMAND ogl-viewer-render-world-tests)

    add_executable(ogl-viewer-live-app-tests tests/live_app_tests.cpp)
    target_link_libraries(ogl-viewer-live-app-tests PRIVATE OpenGenesis::ViewerCore)
    add_test(NAME viewer-live-app COMMAND ogl-viewer-live-app-tests)

    add_executable(ogl-viewer-terrain-patch-tests tests/terrain_patch_tests.cpp)
    target_link_libraries(ogl-viewer-terrain-patch-tests PRIVATE OpenGenesis::ViewerCore)
    add_test(NAME viewer-terrain-patch COMMAND ogl-viewer-terrain-patch-tests)

    add_executable(ogl-viewer-login-form-tests tests/login_form_tests.cpp)
    target_link_libraries(ogl-viewer-login-form-tests PRIVATE OpenGenesis::ViewerCore)
    add_test(NAME viewer-login-form COMMAND ogl-viewer-login-form-tests)

    add_test(NAME viewer-app-help COMMAND ogl-viewer --help)
endif()
