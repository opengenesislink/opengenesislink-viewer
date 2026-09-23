#include "opengenesislink/viewer/input/avatar_controller.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

using namespace ogl::viewer;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

bool close(double left, double right) {
    return std::abs(left - right) < 0.000001;
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

world::Transform transform_fixture() {
    world::Transform transform;
    transform.position = {100.0, 200.0, 25.0};
    transform.rotation = {0.0, 0.0, 0.0};
    return transform;
}

void test_forward_heading() {
    const auto request =
        input::make_avatar_control_request(
            1U,
            transform_fixture(),
            {
                .forward = 1.0,
                .right = 0.0,
                .heading_degrees = 90.0,
                .speed = 4.0,
                .command_seconds = 0.1,
            });

    require(request.client_sequence == 1U,
            "avatar command sequence mismatch");
    require(close(request.velocity.x, 0.0),
            "90 degree forward velocity x mismatch");
    require(close(request.velocity.y, 4.0),
            "90 degree forward velocity y mismatch");
    require(close(request.pose.x, 100.0),
            "90 degree target x mismatch");
    require(close(request.pose.y, 200.4),
            "90 degree target y mismatch");
    require(close(request.pose.rz, 90.0),
            "avatar heading mismatch");
}

void test_diagonal_is_normalized() {
    const auto request =
        input::make_avatar_control_request(
            2U,
            transform_fixture(),
            {
                .forward = 1.0,
                .right = 1.0,
                .heading_degrees = 0.0,
                .speed = 4.0,
                .command_seconds = 0.1,
            });

    const auto speed =
        std::sqrt(
            request.velocity.x * request.velocity.x +
            request.velocity.y * request.velocity.y);
    require(close(speed, 4.0),
            "diagonal avatar velocity exceeded configured speed");
}

void test_stop_command_keeps_position() {
    const auto request =
        input::make_avatar_control_request(
            3U,
            transform_fixture(),
            {
                .forward = 0.0,
                .right = 0.0,
                .heading_degrees = 45.0,
                .speed = 4.0,
                .command_seconds = 0.1,
            });

    require(close(request.pose.x, 100.0) &&
            close(request.pose.y, 200.0),
            "stop command changed avatar position");
    require(close(request.velocity.x, 0.0) &&
            close(request.velocity.y, 0.0),
            "stop command retained velocity");
}

void test_validation() {
    require_throws(
        [] {
            (void)input::make_avatar_control_request(
                0U,
                transform_fixture(),
                {});
        },
        "zero movement sequence must fail");

    require_throws(
        [] {
            (void)input::make_avatar_control_request(
                1U,
                transform_fixture(),
                {
                    .speed = 30.0,
                });
        },
        "excessive movement speed must fail");
}

} // namespace

int main() {
    try {
        test_forward_heading();
        test_diagonal_is_normalized();
        test_stop_command_keeps_position();
        test_validation();
        std::cout
            << "OpenGenesisLINK Viewer avatar controller tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cerr
            << "Avatar controller test failure: "
            << ex.what()
            << "\n";
        return EXIT_FAILURE;
    }
}
