#include "opengenesislink/viewer/scene/frame.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ogl::viewer::scene {
namespace {

void put_u16(std::vector<std::byte>& out, std::uint16_t value) {
    out.push_back(std::byte{static_cast<unsigned char>((value >> 8U) & 0xffU)});
    out.push_back(std::byte{static_cast<unsigned char>(value & 0xffU)});
}

void put_u32(std::vector<std::byte>& out, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        out.push_back(std::byte{static_cast<unsigned char>(
            (value >> static_cast<unsigned>(shift)) & 0xffU)});
    }
}

std::uint16_t get_u16(std::span<const std::byte> input, std::size_t offset) {
    return static_cast<std::uint16_t>(
        (std::to_integer<unsigned>(input[offset]) << 8U) |
        std::to_integer<unsigned>(input[offset + 1U]));
}

std::uint32_t get_u32(std::span<const std::byte> input, std::size_t offset) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4U; ++i) {
        value = (value << 8U) |
                static_cast<std::uint32_t>(std::to_integer<unsigned>(input[offset + i]));
    }
    return value;
}

std::uint32_t validate_header(std::span<const std::byte> bytes) {
    if (bytes.size() < kOgl1HeaderSize) {
        throw std::runtime_error("Short OGL1 frame header");
    }
    if (!std::equal(kOgl1Magic.begin(), kOgl1Magic.end(), bytes.begin())) {
        throw std::runtime_error("Invalid OGL1 magic");
    }
    if (get_u16(bytes, 4U) != kOgl1ProtocolVersion) {
        throw std::runtime_error("Unsupported OGL1 protocol version");
    }

    const auto payload_size = get_u32(bytes, 12U);
    if (payload_size > kOgl1MaxPayloadSize) {
        throw std::runtime_error("OGL1 payload exceeds 1 MiB limit");
    }
    return payload_size;
}

} // namespace

std::vector<std::byte> encode_frame(const Frame& frame) {
    if (frame.payload.size() > kOgl1MaxPayloadSize) {
        throw std::runtime_error("OGL1 payload exceeds 1 MiB limit");
    }
    if (frame.payload.size() >
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        throw std::runtime_error("OGL1 payload length cannot be encoded");
    }

    std::vector<std::byte> output;
    output.reserve(kOgl1HeaderSize + frame.payload.size());
    output.insert(output.end(), kOgl1Magic.begin(), kOgl1Magic.end());
    put_u16(output, kOgl1ProtocolVersion);
    put_u16(output, static_cast<std::uint16_t>(frame.type));
    put_u32(output, frame.request_id);
    put_u32(output, static_cast<std::uint32_t>(frame.payload.size()));
    output.insert(output.end(), frame.payload.begin(), frame.payload.end());
    return output;
}

Frame decode_frame(std::span<const std::byte> bytes) {
    const auto payload_size = validate_header(bytes);
    const auto expected_size =
        kOgl1HeaderSize + static_cast<std::size_t>(payload_size);
    if (bytes.size() != expected_size) {
        throw std::runtime_error("Invalid OGL1 frame size");
    }

    return {
        .type = static_cast<MessageType>(get_u16(bytes, 6U)),
        .request_id = get_u32(bytes, 8U),
        .payload = std::vector<std::byte>(
            bytes.begin() + static_cast<std::ptrdiff_t>(kOgl1HeaderSize),
            bytes.end()),
    };
}

std::string payload_as_string(const Frame& frame) {
    return {
        reinterpret_cast<const char*>(frame.payload.data()),
        frame.payload.size(),
    };
}

std::vector<std::byte> payload_from_string(std::string_view value) {
    const auto* begin = reinterpret_cast<const std::byte*>(value.data());
    return {begin, begin + value.size()};
}

std::vector<Frame> FrameStreamDecoder::feed(std::span<const std::byte> bytes) {
    buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());

    std::vector<Frame> frames;
    while (buffer_.size() >= kOgl1HeaderSize) {
        const std::span<const std::byte> header{buffer_.data(), kOgl1HeaderSize};
        const auto payload_size = validate_header(header);
        const auto frame_size =
            kOgl1HeaderSize + static_cast<std::size_t>(payload_size);

        if (buffer_.size() < frame_size) {
            break;
        }

        frames.push_back(decode_frame(
            std::span<const std::byte>{buffer_.data(), frame_size}));
        buffer_.erase(
            buffer_.begin(),
            buffer_.begin() + static_cast<std::ptrdiff_t>(frame_size));
    }

    if (buffer_.size() > kOgl1HeaderSize + kOgl1MaxPayloadSize) {
        throw std::runtime_error("OGL1 receive buffer exceeded frame bound");
    }

    return frames;
}

void FrameStreamDecoder::reset() noexcept {
    buffer_.clear();
}

std::size_t FrameStreamDecoder::buffered_bytes() const noexcept {
    return buffer_.size();
}

} // namespace ogl::viewer::scene
