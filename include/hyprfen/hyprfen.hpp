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

struct Components {
    std::uint64_t occupied = 0;
    std::uint64_t white_occupied = 0;
    std::uint64_t pawns = 0;
    std::uint64_t knights = 0;
    std::uint64_t bishops = 0;
    std::uint64_t rooks = 0;
    std::uint64_t queens = 0;
    bool white_to_move = true;
    std::uint8_t castling_bits = 0;
    std::uint8_t ep_file = 0;
    bool has_ep_file = false;
    std::uint64_t halfmove_clock = 0;
    std::uint64_t fullmove_number = 1;
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

[[nodiscard]] std::vector<std::uint8_t> encode_components(const Components& components);

[[nodiscard]] std::vector<std::uint8_t> encode_fen(std::string_view fen);

[[nodiscard]] Components decode_components(std::span<const std::uint8_t> data);

[[nodiscard]] std::string decode_fen(std::span<const std::uint8_t> data);

[[nodiscard]] inline std::string decode_fen(const std::vector<std::uint8_t>& data) {
    return decode_fen(std::span<const std::uint8_t>(data));
}

[[nodiscard]] EncodeStats encoding_stats(std::string_view fen);

}  // namespace hyprfen
