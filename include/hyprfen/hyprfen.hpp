#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace hyprfen {

struct EncodeStats {
    std::size_t encoded_bytes;
    std::size_t encoded_bits;
    std::size_t occupancy_count;
};

class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class UnsupportedPositionError : public Error {
public:
    using Error::Error;
};

class MalformedEncodingError : public Error {
public:
    using Error::Error;
};

[[nodiscard]] std::vector<std::uint8_t> encode_fen(std::string_view fen);

[[nodiscard]] std::string decode_fen(std::span<const std::uint8_t> data);

[[nodiscard]] inline std::string decode_fen(const std::vector<std::uint8_t>& data) {
    return decode_fen(std::span<const std::uint8_t>(data));
}

[[nodiscard]] EncodeStats encoding_stats(std::string_view fen);

}  // namespace hyprfen
