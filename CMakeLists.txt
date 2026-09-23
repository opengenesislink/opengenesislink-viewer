cmake_minimum_required(VERSION 3.25)
project(OpenGenesisLINKViewer VERSION 0.2.0 LANGUAGES CXX)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" OGL_VIEWER_VERSION_LABEL)
string(STRIP "${OGL_VIEWER_VERSION_LABEL}" OGL_VIEWER_VERSION_LABEL)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OGL_VIEWER_BUILD_TESTS "Build OpenGenesisLINK Viewer tests" ON)
option(OGL_VIEWER_ENABLE_WERROR "Treat warnings as errors" OFF)

find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(opengenesis-viewer-core
    src/core/curl_http_transport.cpp
    src/core/release_discovery.cpp
    src/core/auth_client.cpp
    src/core/viewer_bootstrap.cpp
    src/scene/frame.cpp
    src/scene/session_protocol.cpp
    src/scene/transport.cpp
    src/scene/scene_session.cpp
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

add_executable(ogl-viewer-contract-smoke apps/contract-smoke/main.cpp)
target_link_libraries(ogl-viewer-contract-smoke PRIVATE OpenGenesis::ViewerCore)

install(TARGETS opengenesis-viewer-core ogl-viewer-contract-smoke
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY include/ DESTINATION include)

include(CTest)
if(BUILD_TESTING AND OGL_VIEWER_BUILD_TESTS)
    add_executable(ogl-viewer-core-tests tests/core_contract_tests.cpp)
    target_link_libraries(ogl-viewer-core-tests PRIVATE OpenGenesis::ViewerCore)
    add_test(NAME viewer-core-contracts COMMAND ogl-viewer-core-tests)

    add_executable(ogl-viewer-scene-tests tests/scene_protocol_tests.cpp)
    target_link_libraries(ogl-viewer-scene-tests PRIVATE OpenGenesis::ViewerCore)
    add_test(NAME viewer-scene-protocol COMMAND ogl-viewer-scene-tests)

    add_executable(ogl-viewer-scene-session-tests tests/scene_session_tests.cpp)
    target_link_libraries(ogl-viewer-scene-session-tests PRIVATE OpenGenesis::ViewerCore)
    add_test(NAME viewer-scene-session COMMAND ogl-viewer-scene-session-tests)
endif()
