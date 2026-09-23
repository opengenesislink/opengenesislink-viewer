#include "opengenesislink/viewer/app/command_console.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

using namespace ogl::viewer::app;

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

void test_plain_chat_and_quotes() {
    const auto chat =
        parse_console_command(
            "Hello virtual world");
    require(
        chat.kind ==
            ConsoleCommandKind::local_chat,
        "plain text must become local chat");
    require(
        chat.arguments.size() == 1U &&
        chat.arguments[0] ==
            "Hello virtual world",
        "plain chat text mismatch");

    const auto folder =
        parse_console_command(
            "/inventory folder root \"My Clothes\"");
    require(
        folder.kind ==
            ConsoleCommandKind::inventory_folder_create,
        "Inventory folder command mismatch");
    require(
        folder.arguments.size() == 2U &&
        folder.arguments[1] == "My Clothes",
        "quoted command token mismatch");
}

void test_feature_commands() {
    require(
        parse_console_command(
            "/social refresh").kind ==
            ConsoleCommandKind::social_refresh,
        "social refresh mismatch");
    require(
        parse_console_command(
            "/wear shirt item asset").kind ==
            ConsoleCommandKind::wearable_set,
        "wear command mismatch");
    require(
        parse_console_command(
            "/group info g1").kind ==
            ConsoleCommandKind::group_details,
        "group info mismatch");
    require(
        parse_console_command(
            "/parcel access-list p1").kind ==
            ConsoleCommandKind::parcel_access_list,
        "parcel access list mismatch");
    require(
        parse_console_command(
            "/tp region-2 128 128 25").kind ==
            ConsoleCommandKind::teleport,
        "teleport command mismatch");
    require(
        parse_console_command(
            "/handoff region-3").kind ==
            ConsoleCommandKind::handoff,
        "handoff command mismatch");
    require(
        parse_console_command(
            "/build shape 10 box").kind ==
            ConsoleCommandKind::build_shape,
        "build shape command mismatch");
    require(
        parse_console_command(
            "/terrain set 5 6 22.5").kind ==
            ConsoleCommandKind::terrain_set,
        "terrain command mismatch");
}

void test_invalid_commands() {
    require_throws(
        [] {
            (void)parse_console_command(
                "/unknown x");
        },
        "unknown command must fail");

    require_throws(
        [] {
            (void)parse_console_command(
                "/dm user-only");
        },
        "incomplete DM command must fail");

    require_throws(
        [] {
            (void)parse_console_command(
                "/inventory folder root \"unterminated");
        },
        "unterminated quote must fail");
}

} // namespace

int main() {
    try {
        test_plain_chat_and_quotes();
        test_feature_commands();
        test_invalid_commands();
        std::cout
            << "OpenGenesisLINK Viewer command console tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr
            << "Command console test failure: "
            << ex.what()
            << "\n";
        return EXIT_FAILURE;
    }
}
