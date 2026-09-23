#pragma once

#include "opengenesislink/viewer/scene/frame.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <string>
#include <string_view>

namespace ogl::viewer::scene {

struct SceneEndpoint {
    std::string host;
    std::uint16_t port = 0;
};

[[nodiscard]] SceneEndpoint parse_scene_endpoint(std::string_view value);

class ByteStream {
public:
    virtual ~ByteStream() = default;

    virtual void connect(const SceneEndpoint& endpoint) = 0;
    virtual void send_all(std::span<const std::byte> bytes) = 0;
    [[nodiscard]] virtual std::size_t receive_some(std::span<std::byte> buffer) = 0;
    virtual void close() noexcept = 0;
    [[nodiscard]] virtual bool is_open() const noexcept = 0;
};

class TcpByteStream final : public ByteStream {
public:
    explicit TcpByteStream(
        std::chrono::milliseconds io_timeout = std::chrono::seconds(15));
    ~TcpByteStream() override;

    TcpByteStream(const TcpByteStream&) = delete;
    TcpByteStream& operator=(const TcpByteStream&) = delete;
    TcpByteStream(TcpByteStream&& other) noexcept;
    TcpByteStream& operator=(TcpByteStream&& other) noexcept;

    void connect(const SceneEndpoint& endpoint) override;
    void send_all(std::span<const std::byte> bytes) override;
    [[nodiscard]] std::size_t receive_some(std::span<std::byte> buffer) override;
    void close() noexcept override;
    [[nodiscard]] bool is_open() const noexcept override;

private:
    static constexpr std::uintptr_t invalid_handle_ = UINTPTR_MAX;

    std::uintptr_t handle_ = invalid_handle_;
    std::chrono::milliseconds io_timeout_;
};

class FrameChannel {
public:
    explicit FrameChannel(ByteStream& stream);

    void send(const Frame& frame);
    [[nodiscard]] Frame receive();
    void close() noexcept;
    [[nodiscard]] bool is_open() const noexcept;

private:
    ByteStream& stream_;
    FrameStreamDecoder decoder_;
    std::deque<Frame> pending_;
};

} // namespace ogl::viewer::scene
