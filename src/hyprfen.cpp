#include "hyprfen/hyprfen.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hyprfen {
namespace {

constexpr std::uint8_t kWhiteKingside = 1u << 0;
constexpr std::uint8_t kWhiteQueenside = 1u << 1;
constexpr std::uint8_t kBlackKingside = 1u << 2;
constexpr std::uint8_t kBlackQueenside = 1u << 3;

struct BoardState {
    std::uint64_t occupied = 0;
    std::uint64_t white_occupied = 0;
    std::uint64_t pawns = 0;
    std::uint64_t knights = 0;
    std::uint64_t bishops = 0;
    std::uint64_t rooks = 0;
    std::uint64_t queens = 0;
    bool white_to_move = true;
    std::uint8_t castling_bits = 0;
    std::optional<std::uint8_t> ep_file;
    std::uint64_t halfmove_clock = 0;
    std::uint64_t fullmove_number = 1;
};

[[nodiscard]] std::vector<std::string_view> split_fields(std::string_view text) {
    std::vector<std::string_view> fields;
    std::size_t pos = 0;
    while (pos < text.size()) {
        while (pos < text.size() && text[pos] == ' ') {
            ++pos;
        }
        if (pos >= text.size()) {
            break;
        }
        std::size_t next = text.find(' ', pos);
        if (next == std::string_view::npos) {
            fields.push_back(text.substr(pos));
            break;
        }
        fields.push_back(text.substr(pos, next - pos));
        pos = next + 1;
    }
    return fields;
}

[[nodiscard]] std::uint64_t parse_u64(std::string_view text, const char* field_name) {
    if (text.empty()) {
        throw Error(std::string("missing value for ") + field_name);
    }
    std::uint64_t value = 0;
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        throw Error(std::string("invalid numeric value for ") + field_name + ": " +
                    std::string(text));
    }
    return value;
}

[[nodiscard]] std::uint64_t bit_for_square(int square) {
    return std::uint64_t{1} << square;
}

[[nodiscard]] int popcount(std::uint64_t value) {
    return std::popcount(value);
}

[[nodiscard]] std::uint64_t pext(std::uint64_t value, std::uint64_t mask) {
    std::uint64_t out = 0;
    std::uint64_t out_bit = 1;
    while (mask != 0) {
        const std::uint64_t lsb = mask & (~mask + 1);
        if ((value & lsb) != 0) {
            out |= out_bit;
        }
        mask ^= lsb;
        out_bit <<= 1;
    }
    return out;
}

[[nodiscard]] std::uint64_t pdep(std::uint64_t value, std::uint64_t mask) {
    std::uint64_t out = 0;
    std::uint64_t in_bit = 1;
    while (mask != 0) {
        const std::uint64_t lsb = mask & (~mask + 1);
        if ((value & in_bit) != 0) {
            out |= lsb;
        }
        mask ^= lsb;
        in_bit <<= 1;
    }
    return out;
}

class BitWriter {
public:
    void write_bits(std::uint64_t value, std::size_t nbits) {
        if (nbits > 64) {
            throw Error("nbits must be <= 64");
        }
        if (nbits == 0) {
            return;
        }
        if (nbits < 64 && (value >> nbits) != 0) {
            throw Error("value does not fit in requested bit width");
        }
        std::size_t remaining = nbits;
        while (remaining > 0) {
            const std::size_t byte_index = nbits_ / 8;
            const std::size_t bit_index = nbits_ % 8;
            if (byte_index == bytes_.size()) {
                bytes_.push_back(0);
            }
            const std::size_t chunk_bits = std::min<std::size_t>(remaining, 8 - bit_index);
            const std::uint8_t chunk_mask =
                static_cast<std::uint8_t>((std::uint64_t{1} << chunk_bits) - 1);
            const std::uint8_t chunk = static_cast<std::uint8_t>(value & chunk_mask);
            bytes_[byte_index] |= static_cast<std::uint8_t>(chunk << bit_index);
            value >>= chunk_bits;
            nbits_ += chunk_bits;
            remaining -= chunk_bits;
        }
    }

    void write_bool(bool value) {
        write_bits(value ? 1u : 0u, 1);
    }

    void write_varuint(std::uint64_t value) {
        while (true) {
            std::uint8_t byte = static_cast<std::uint8_t>(value & 0x7Fu);
            value >>= 7;
            if (value != 0) {
                byte = static_cast<std::uint8_t>(byte | 0x80u);
                write_bits(byte, 8);
            } else {
                write_bits(byte, 8);
                break;
            }
        }
    }

    [[nodiscard]] std::size_t nbits() const {
        return nbits_;
    }

    [[nodiscard]] std::vector<std::uint8_t> to_bytes() const {
        return bytes_;
    }

private:
    std::vector<std::uint8_t> bytes_;
    std::size_t nbits_ = 0;
};

class BitReader {
public:
    explicit BitReader(std::span<const std::uint8_t> data) : data_(data) {}

    [[nodiscard]] std::size_t remaining_bits() const {
        return data_.size() * 8 - offset_bits_;
    }

    std::uint64_t read_bits(std::size_t nbits) {
        if (nbits > 64) {
            throw MalformedEncodingError("requested bit width exceeds 64");
        }
        if (offset_bits_ + nbits > data_.size() * 8) {
            throw MalformedEncodingError("not enough bits remaining in stream");
        }
        std::uint64_t value = 0;
        std::size_t shift = 0;
        std::size_t remaining = nbits;
        while (remaining > 0) {
            const std::size_t byte_index = offset_bits_ / 8;
            const std::size_t bit_index = offset_bits_ % 8;
            const std::size_t chunk_bits = std::min<std::size_t>(remaining, 8 - bit_index);
            const std::uint8_t byte = data_[byte_index];
            const std::uint8_t mask =
                static_cast<std::uint8_t>((std::uint64_t{1} << chunk_bits) - 1);
            const std::uint64_t chunk = (byte >> bit_index) & mask;
            value |= chunk << shift;
            offset_bits_ += chunk_bits;
            shift += chunk_bits;
            remaining -= chunk_bits;
        }
        return value;
    }

    [[nodiscard]] bool read_bool() {
        return read_bits(1) != 0;
    }

    [[nodiscard]] std::uint64_t read_varuint() {
        std::size_t shift = 0;
        std::uint64_t value = 0;
        while (true) {
            const std::uint64_t byte = read_bits(8);
            value |= (byte & 0x7Fu) << shift;
            if ((byte & 0x80u) == 0) {
                return value;
            }
            shift += 7;
            if (shift > 63) {
                throw MalformedEncodingError("varuint is too large or malformed");
            }
        }
    }

private:
    std::span<const std::uint8_t> data_;
    std::size_t offset_bits_ = 0;
};

void set_piece(BoardState& board, int square, char piece) {
    const std::uint64_t bit = bit_for_square(square);
    board.occupied |= bit;
    switch (piece) {
        case 'P':
            board.white_occupied |= bit;
            board.pawns |= bit;
            return;
        case 'N':
            board.white_occupied |= bit;
            board.knights |= bit;
            return;
        case 'B':
            board.white_occupied |= bit;
            board.bishops |= bit;
            return;
        case 'R':
            board.white_occupied |= bit;
            board.rooks |= bit;
            return;
        case 'Q':
            board.white_occupied |= bit;
            board.queens |= bit;
            return;
        case 'K':
            board.white_occupied |= bit;
            return;
        case 'p':
            board.pawns |= bit;
            return;
        case 'n':
            board.knights |= bit;
            return;
        case 'b':
            board.bishops |= bit;
            return;
        case 'r':
            board.rooks |= bit;
            return;
        case 'q':
            board.queens |= bit;
            return;
        case 'k':
            return;
        default:
            throw Error(std::string("invalid piece character: ") + piece);
    }
}

[[nodiscard]] BoardState parse_fen(std::string_view fen) {
    const std::vector<std::string_view> fields = split_fields(fen);
    if (fields.size() != 6) {
        throw Error("expected 6 FEN fields");
    }

    BoardState board;

    {
        const std::string_view placement = fields[0];
        std::size_t start = 0;
        for (int row = 0; row < 8; ++row) {
            const std::size_t slash = placement.find('/', start);
            const std::string_view rank = row == 7
                                              ? placement.substr(start)
                                              : placement.substr(start, slash - start);
            int file = 0;
            for (char ch : rank) {
                if (ch >= '1' && ch <= '8') {
                    file += ch - '0';
                    continue;
                }
                if (file >= 8) {
                    throw Error("too many squares in FEN rank");
                }
                const int square = (7 - row) * 8 + file;
                set_piece(board, square, ch);
                ++file;
            }
            if (file != 8) {
                throw Error("rank does not cover 8 files");
            }
            if (row < 7) {
                if (slash == std::string_view::npos) {
                    throw Error("expected 8 ranks in FEN placement");
                }
                start = slash + 1;
            }
        }
        if (placement.find('/', start) != std::string_view::npos) {
            throw Error("too many ranks in FEN placement");
        }
    }

    if (fields[1] == "w") {
        board.white_to_move = true;
    } else if (fields[1] == "b") {
        board.white_to_move = false;
    } else {
        throw Error("active color must be 'w' or 'b'");
    }

    if (fields[2] == "-") {
        board.castling_bits = 0;
    } else {
        for (char ch : fields[2]) {
            switch (ch) {
                case 'K':
                    board.castling_bits = static_cast<std::uint8_t>(board.castling_bits |
                                                                    kWhiteKingside);
                    break;
                case 'Q':
                    board.castling_bits = static_cast<std::uint8_t>(board.castling_bits |
                                                                    kWhiteQueenside);
                    break;
                case 'k':
                    board.castling_bits = static_cast<std::uint8_t>(board.castling_bits |
                                                                    kBlackKingside);
                    break;
                case 'q':
                    board.castling_bits = static_cast<std::uint8_t>(board.castling_bits |
                                                                    kBlackQueenside);
                    break;
                default:
                    throw UnsupportedPositionError("only standard KQkq castling notation is supported");
            }
        }
    }

    if (fields[3] != "-") {
        if (fields[3].size() != 2 || fields[3][0] < 'a' || fields[3][0] > 'h' ||
            (fields[3][1] != '3' && fields[3][1] != '6')) {
            throw Error("invalid en-passant square");
        }
        board.ep_file = static_cast<std::uint8_t>(fields[3][0] - 'a');
    }

    board.halfmove_clock = parse_u64(fields[4], "halfmove clock");
    board.fullmove_number = parse_u64(fields[5], "fullmove number");
    if (board.fullmove_number == 0) {
        throw Error("fullmove number must be positive");
    }
    return board;
}

[[nodiscard]] std::vector<std::uint8_t> encode_state(
    const BoardState& board,
    EncodeStats* stats_out = nullptr
) {
    BitWriter writer;
    writer.write_bits(board.occupied, 64);

    const std::size_t n_occupied = static_cast<std::size_t>(popcount(board.occupied));
    writer.write_bits(pext(board.white_occupied, board.occupied), n_occupied);
    writer.write_bits(pext(board.pawns, board.occupied), n_occupied);

    std::uint64_t remainder = board.occupied & ~board.pawns;
    writer.write_bits(pext(board.knights, remainder), static_cast<std::size_t>(popcount(remainder)));
    remainder &= ~board.knights;
    writer.write_bits(pext(board.bishops, remainder), static_cast<std::size_t>(popcount(remainder)));
    remainder &= ~board.bishops;
    writer.write_bits(pext(board.rooks, remainder), static_cast<std::size_t>(popcount(remainder)));
    remainder &= ~board.rooks;
    writer.write_bits(pext(board.queens, remainder), static_cast<std::size_t>(popcount(remainder)));

    writer.write_bool(board.white_to_move);
    writer.write_bits(board.castling_bits, 4);
    writer.write_bool(board.ep_file.has_value());
    if (board.ep_file.has_value()) {
        writer.write_bits(*board.ep_file, 3);
    }
    writer.write_varuint(board.halfmove_clock);
    writer.write_varuint(board.fullmove_number);

    std::vector<std::uint8_t> encoded = writer.to_bytes();
    if (stats_out != nullptr) {
        *stats_out = EncodeStats{
            .encoded_bytes = encoded.size(),
            .encoded_bits = writer.nbits(),
            .occupancy_count = n_occupied,
        };
    }
    return encoded;
}

[[nodiscard]] char piece_at(
    std::uint64_t white_occupied,
    std::uint64_t pawns,
    std::uint64_t knights,
    std::uint64_t bishops,
    std::uint64_t rooks,
    std::uint64_t queens,
    std::uint64_t kings,
    int square
) {
    const std::uint64_t bit = bit_for_square(square);
    const bool is_white = (white_occupied & bit) != 0;
    if ((pawns & bit) != 0) {
        return is_white ? 'P' : 'p';
    }
    if ((knights & bit) != 0) {
        return is_white ? 'N' : 'n';
    }
    if ((bishops & bit) != 0) {
        return is_white ? 'B' : 'b';
    }
    if ((rooks & bit) != 0) {
        return is_white ? 'R' : 'r';
    }
    if ((queens & bit) != 0) {
        return is_white ? 'Q' : 'q';
    }
    if ((kings & bit) != 0) {
        return is_white ? 'K' : 'k';
    }
    return '\0';
}

[[nodiscard]] std::string castling_string(std::uint8_t bits) {
    std::string out;
    if ((bits & kWhiteKingside) != 0) {
        out.push_back('K');
    }
    if ((bits & kWhiteQueenside) != 0) {
        out.push_back('Q');
    }
    if ((bits & kBlackKingside) != 0) {
        out.push_back('k');
    }
    if ((bits & kBlackQueenside) != 0) {
        out.push_back('q');
    }
    return out.empty() ? "-" : out;
}

[[nodiscard]] std::string decode_state_to_fen(
    std::uint64_t occupied,
    std::uint64_t white_occupied,
    std::uint64_t pawns,
    std::uint64_t knights,
    std::uint64_t bishops,
    std::uint64_t rooks,
    std::uint64_t queens,
    bool white_to_move,
    std::uint8_t castling_bits,
    std::optional<std::uint8_t> ep_file,
    std::uint64_t halfmove_clock,
    std::uint64_t fullmove_number
) {
    const std::uint64_t black_occupied = occupied ^ white_occupied;
    const std::uint64_t kings = (occupied & ~pawns & ~knights & ~bishops & ~rooks) & ~queens;

    std::ostringstream out;
    for (int rank = 7; rank >= 0; --rank) {
        int empty_count = 0;
        for (int file = 0; file < 8; ++file) {
            const int square = rank * 8 + file;
            const char piece = piece_at(
                white_occupied,
                pawns,
                knights,
                bishops,
                rooks,
                queens,
                kings,
                square
            );
            if (piece == '\0') {
                ++empty_count;
                continue;
            }
            if (empty_count != 0) {
                out << empty_count;
                empty_count = 0;
            }
            out << piece;
        }
        if (empty_count != 0) {
            out << empty_count;
        }
        if (rank != 0) {
            out << '/';
        }
    }

    out << ' ' << (white_to_move ? 'w' : 'b') << ' ' << castling_string(castling_bits) << ' ';
    if (ep_file.has_value()) {
        out << static_cast<char>('a' + *ep_file) << (white_to_move ? '6' : '3');
    } else {
        out << '-';
    }
    out << ' ' << halfmove_clock << ' ' << fullmove_number;
    return out.str();
}

}  // namespace

std::vector<std::uint8_t> encode_fen(std::string_view fen) {
    const BoardState board = parse_fen(fen);
    return encode_state(board);
}

std::string decode_fen(std::span<const std::uint8_t> data) {
    BitReader reader(data);

    const std::uint64_t occupied = reader.read_bits(64);
    const std::size_t n_occupied = static_cast<std::size_t>(popcount(occupied));

    const std::uint64_t white_occupied = pdep(reader.read_bits(n_occupied), occupied);
    const std::uint64_t pawns = pdep(reader.read_bits(n_occupied), occupied);

    std::uint64_t remainder = occupied & ~pawns;
    const std::uint64_t knights = pdep(reader.read_bits(static_cast<std::size_t>(popcount(remainder))), remainder);
    remainder &= ~knights;
    const std::uint64_t bishops = pdep(reader.read_bits(static_cast<std::size_t>(popcount(remainder))), remainder);
    remainder &= ~bishops;
    const std::uint64_t rooks = pdep(reader.read_bits(static_cast<std::size_t>(popcount(remainder))), remainder);
    remainder &= ~rooks;
    const std::uint64_t queens = pdep(reader.read_bits(static_cast<std::size_t>(popcount(remainder))), remainder);

    const bool white_to_move = reader.read_bool();
    const std::uint8_t castling_bits = static_cast<std::uint8_t>(reader.read_bits(4));
    std::optional<std::uint8_t> ep_file;
    if (reader.read_bool()) {
        const std::uint64_t raw_ep_file = reader.read_bits(3);
        if (raw_ep_file > 7) {
            throw MalformedEncodingError("invalid en-passant file");
        }
        ep_file = static_cast<std::uint8_t>(raw_ep_file);
    }
    const std::uint64_t halfmove_clock = reader.read_varuint();
    const std::uint64_t fullmove_number = reader.read_varuint();

    const std::size_t remaining = reader.remaining_bits();
    if (remaining > 7) {
        throw MalformedEncodingError("too many unread bits remain after decoding");
    }
    if (remaining != 0 && reader.read_bits(remaining) != 0) {
        throw MalformedEncodingError("non-zero padding bits found at end of stream");
    }

    return decode_state_to_fen(
        occupied,
        white_occupied,
        pawns,
        knights,
        bishops,
        rooks,
        queens,
        white_to_move,
        castling_bits,
        ep_file,
        halfmove_clock,
        fullmove_number
    );
}

EncodeStats encoding_stats(std::string_view fen) {
    const BoardState board = parse_fen(fen);
    EncodeStats stats{};
    static_cast<void>(encode_state(board, &stats));
    return stats;
}

}  // namespace hyprfen
