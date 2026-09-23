#include "opengenesislink/viewer/scene/transport.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <climits>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace ogl::viewer::scene {
namespace {

#if defined(_WIN32)
using NativeSocket = SOCKET;
inline constexpr NativeSocket kInvalidSocket = INVALID_SOCKET;

struct WinsockRuntime {
    WinsockRuntime() {
        WSADATA data{};
        const auto result = WSAStartup(MAKEWORD(2, 2), &data);
        if (result != 0) {
            throw std::runtime_error("WSAStartup failed: " + std::to_string(result));
        }
    }
    ~WinsockRuntime() {
        WSACleanup();
    }
};

void ensure_socket_runtime() {
    static WinsockRuntime runtime;
    (void)runtime;
}

int last_socket_error() noexcept {
    return WSAGetLastError();
}

void close_socket(NativeSocket socket) noexcept {
    if (socket != kInvalidSocket) {
        closesocket(socket);
    }
}
#else
using NativeSocket = int;
inline constexpr NativeSocket kInvalidSocket = -1;

void ensure_socket_runtime() {}

int last_socket_error() noexcept {
    return errno;
}

void close_socket(NativeSocket socket) noexcept {
    if (socket != kInvalidSocket) {
        ::close(socket);
    }
}
#endif

std::runtime_error socket_error(std::string_view operation, int error) {
    return std::runtime_error(
        std::string(operation) + " failed with socket error " + std::to_string(error));
}

NativeSocket native_from_handle(std::uintptr_t handle) noexcept {
#if defined(_WIN32)
    return static_cast<NativeSocket>(handle);
#else
    if (handle == UINTPTR_MAX) {
        return kInvalidSocket;
    }
    return static_cast<NativeSocket>(handle);
#endif
}

std::uintptr_t handle_from_native(NativeSocket socket) noexcept {
#if defined(_WIN32)
    return static_cast<std::uintptr_t>(socket);
#else
    return socket == kInvalidSocket
        ? UINTPTR_MAX
        : static_cast<std::uintptr_t>(socket);
#endif
}

void configure_io_timeout(
    NativeSocket socket,
    std::chrono::milliseconds timeout) {
    if (timeout.count() <= 0) {
        throw std::invalid_argument("TCP I/O timeout must be positive");
    }

#if defined(_WIN32)
    const auto bounded = std::min<std::int64_t>(
        timeout.count(),
        static_cast<std::int64_t>(std::numeric_limits<DWORD>::max()));
    const DWORD value = static_cast<DWORD>(bounded);
    if (setsockopt(
            socket,
            SOL_SOCKET,
            SO_RCVTIMEO,
            reinterpret_cast<const char*>(&value),
            static_cast<int>(sizeof(value))) == SOCKET_ERROR ||
        setsockopt(
            socket,
            SOL_SOCKET,
            SO_SNDTIMEO,
            reinterpret_cast<const char*>(&value),
            static_cast<int>(sizeof(value))) == SOCKET_ERROR) {
        throw socket_error("setsockopt timeout", last_socket_error());
    }
#else
    const auto milliseconds = timeout.count();
    timeval value{};
    value.tv_sec = static_cast<decltype(value.tv_sec)>(milliseconds / 1000);
    value.tv_usec = static_cast<decltype(value.tv_usec)>((milliseconds % 1000) * 1000);
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &value, sizeof(value)) != 0 ||
        setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &value, sizeof(value)) != 0) {
        throw socket_error("setsockopt timeout", last_socket_error());
    }
#endif
}

} // namespace

SceneEndpoint parse_scene_endpoint(std::string_view value) {
    if (value.empty()) {
        throw std::invalid_argument("Scene endpoint must not be empty");
    }

    constexpr std::string_view prefix = "tcp://";
    if (value.starts_with(prefix)) {
        value.remove_prefix(prefix.size());
    }

    std::string_view host;
    std::string_view port_text;

    if (value.front() == '[') {
        const auto closing = value.find(']');
        if (closing == std::string_view::npos ||
            closing + 1U >= value.size() ||
            value[closing + 1U] != ':') {
            throw std::invalid_argument("Invalid bracketed Scene endpoint");
        }
        host = value.substr(1U, closing - 1U);
        port_text = value.substr(closing + 2U);
    } else {
        const auto separator = value.rfind(':');
        if (separator == std::string_view::npos) {
            throw std::invalid_argument("Scene endpoint must contain host:port");
        }
        host = value.substr(0U, separator);
        port_text = value.substr(separator + 1U);
        if (host.find(':') != std::string_view::npos) {
            throw std::invalid_argument("IPv6 Scene endpoints must use [address]:port");
        }
    }

    if (host.empty() || port_text.empty()) {
        throw std::invalid_argument("Scene endpoint host and port are required");
    }

    std::uint32_t port = 0;
    for (const char digit : port_text) {
        if (digit < '0' || digit > '9') {
            throw std::invalid_argument("Scene endpoint port is not numeric");
        }
        port = port * 10U + static_cast<std::uint32_t>(digit - '0');
        if (port > 65535U) {
            throw std::invalid_argument("Scene endpoint port is out of range");
        }
    }
    if (port == 0U) {
        throw std::invalid_argument("Scene endpoint port must be 1..65535");
    }

    return {
        .host = std::string(host),
        .port = static_cast<std::uint16_t>(port),
    };
}

TcpByteStream::TcpByteStream(std::chrono::milliseconds io_timeout)
    : io_timeout_(io_timeout) {
    if (io_timeout_.count() <= 0) {
        throw std::invalid_argument("TCP I/O timeout must be positive");
    }
    ensure_socket_runtime();
}

TcpByteStream::~TcpByteStream() {
    close();
}

TcpByteStream::TcpByteStream(TcpByteStream&& other) noexcept
    : handle_(std::exchange(other.handle_, invalid_handle_)),
      io_timeout_(other.io_timeout_) {}

TcpByteStream& TcpByteStream::operator=(TcpByteStream&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = std::exchange(other.handle_, invalid_handle_);
        io_timeout_ = other.io_timeout_;
    }
    return *this;
}

void TcpByteStream::connect(const SceneEndpoint& endpoint) {
    close();
    ensure_socket_runtime();

    if (endpoint.host.empty() || endpoint.port == 0U) {
        throw std::invalid_argument("Invalid Scene endpoint");
    }

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* addresses = nullptr;
    const auto service = std::to_string(endpoint.port);
    const auto resolve_result =
        getaddrinfo(endpoint.host.c_str(), service.c_str(), &hints, &addresses);
    if (resolve_result != 0) {
#if defined(_WIN32)
        throw std::runtime_error(
            "Scene endpoint resolution failed: " + std::to_string(resolve_result));
#else
        throw std::runtime_error(
            "Scene endpoint resolution failed: " + std::string(gai_strerror(resolve_result)));
#endif
    }

    struct AddressListGuard {
        addrinfo* value;
        ~AddressListGuard() {
            if (value != nullptr) {
                freeaddrinfo(value);
            }
        }
    } guard{addresses};

    int connect_error = 0;
    for (auto* address = addresses; address != nullptr; address = address->ai_next) {
        const auto socket =
            ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (socket == kInvalidSocket) {
            connect_error = last_socket_error();
            continue;
        }

        if (::connect(
                socket,
                address->ai_addr,
#if defined(_WIN32)
                static_cast<int>(address->ai_addrlen)
#else
                address->ai_addrlen
#endif
                ) == 0) {
            try {
                configure_io_timeout(socket, io_timeout_);
            } catch (...) {
                close_socket(socket);
                throw;
            }
            handle_ = handle_from_native(socket);
            return;
        }

        connect_error = last_socket_error();
        close_socket(socket);
    }

    throw socket_error("Scene TCP connect", connect_error);
}

void TcpByteStream::send_all(std::span<const std::byte> bytes) {
    if (!is_open()) {
        throw std::runtime_error("Scene TCP socket is not connected");
    }

    auto offset = std::size_t{0};
    const auto socket = native_from_handle(handle_);
    while (offset < bytes.size()) {
#if defined(_WIN32)
        const auto remaining = std::min<std::size_t>(
            bytes.size() - offset,
            static_cast<std::size_t>(INT_MAX));
        const auto sent = ::send(
            socket,
            reinterpret_cast<const char*>(bytes.data() + offset),
            static_cast<int>(remaining),
            0);
        if (sent == SOCKET_ERROR) {
            throw socket_error("Scene TCP send", last_socket_error());
        }
#else
        const auto remaining = bytes.size() - offset;
#ifdef MSG_NOSIGNAL
        constexpr int flags = MSG_NOSIGNAL;
#else
        constexpr int flags = 0;
#endif
        const auto sent = ::send(
            socket,
            bytes.data() + offset,
            remaining,
            flags);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw socket_error("Scene TCP send", last_socket_error());
        }
#endif
        if (sent == 0) {
            throw std::runtime_error("Scene TCP connection closed during send");
        }
        offset += static_cast<std::size_t>(sent);
    }
}

std::size_t TcpByteStream::receive_some(std::span<std::byte> buffer) {
    if (!is_open()) {
        throw std::runtime_error("Scene TCP socket is not connected");
    }
    if (buffer.empty()) {
        return 0U;
    }

    const auto socket = native_from_handle(handle_);
#if defined(_WIN32)
    const auto count = std::min<std::size_t>(
        buffer.size(),
        static_cast<std::size_t>(INT_MAX));
    const auto received = ::recv(
        socket,
        reinterpret_cast<char*>(buffer.data()),
        static_cast<int>(count),
        0);
    if (received == SOCKET_ERROR) {
        throw socket_error("Scene TCP receive", last_socket_error());
    }
#else
    std::ptrdiff_t received = 0;
    do {
        received = ::recv(socket, buffer.data(), buffer.size(), 0);
    } while (received < 0 && errno == EINTR);
    if (received < 0) {
        throw socket_error("Scene TCP receive", last_socket_error());
    }
#endif

    if (received == 0) {
        throw std::runtime_error("Scene TCP peer closed the connection");
    }
    return static_cast<std::size_t>(received);
}

void TcpByteStream::close() noexcept {
    if (!is_open()) {
        return;
    }
    close_socket(native_from_handle(handle_));
    handle_ = invalid_handle_;
}

bool TcpByteStream::is_open() const noexcept {
    return handle_ != invalid_handle_;
}

FrameChannel::FrameChannel(ByteStream& stream)
    : stream_(stream) {}

void FrameChannel::send(const Frame& frame) {
    const auto bytes = encode_frame(frame);
    stream_.send_all(bytes);
}

Frame FrameChannel::receive() {
    if (!pending_.empty()) {
        auto frame = std::move(pending_.front());
        pending_.pop_front();
        return frame;
    }

    std::array<std::byte, 64U * 1024U> buffer{};
    while (true) {
        const auto count = stream_.receive_some(buffer);
        const auto frames = decoder_.feed(
            std::span<const std::byte>{buffer.data(), count});
        for (auto frame : frames) {
            pending_.push_back(std::move(frame));
        }
        if (!pending_.empty()) {
            auto frame = std::move(pending_.front());
            pending_.pop_front();
            return frame;
        }
    }
}

void FrameChannel::close() noexcept {
    pending_.clear();
    decoder_.reset();
    stream_.close();
}

bool FrameChannel::is_open() const noexcept {
    return stream_.is_open();
}

} // namespace ogl::viewer::scene
