#include "hyprfen/hyprfen.hpp"

#include <cstdint>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct GoldenCase {
    std::string_view fen;
    std::string_view hex;
    std::size_t bytes;
};

std::string to_hex(std::span<const std::uint8_t> bytes) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const std::uint8_t byte : bytes) {
        out << std::setw(2) << static_cast<unsigned>(byte);
    }
    return out.str();
}

std::vector<std::uint8_t> from_hex(std::string_view hex) {
    if (hex.size() % 2 != 0) {
        throw std::runtime_error("hex input must have an even length");
    }
    const auto hex_digit = [](char ch) -> std::uint8_t {
        if (ch >= '0' && ch <= '9') {
            return static_cast<std::uint8_t>(ch - '0');
        }
        if (ch >= 'a' && ch <= 'f') {
            return static_cast<std::uint8_t>(ch - 'a' + 10);
        }
        throw std::runtime_error("invalid hex digit");
    };

    std::vector<std::uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        bytes.push_back(static_cast<std::uint8_t>((hex_digit(hex[i]) << 4) | hex_digit(hex[i + 1])));
    }
    return bytes;
}

void expect(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

template <typename Exception, typename Fn>
void expect_throws(Fn&& fn, std::string_view message) {
    bool threw = false;
    try {
        fn();
    } catch (const Exception&) {
        threw = true;
    }
    expect(threw, message);
}

void test_bitstream_vectors(std::string_view path) {
    std::ifstream input{std::string(path)};
    expect(input.good(), "could not open bitstream vector file");

    std::size_t line_number = 0;
    std::size_t cases = 0;
    std::string line;
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.front() == '#') {
            continue;
        }
        const std::size_t separator = line.find('\t');
        if (separator == std::string::npos) {
            throw std::runtime_error("invalid bitstream vector at line " + std::to_string(line_number));
        }
        const std::string_view fen(line.data(), separator);
        const std::vector<std::uint8_t> expected = from_hex(
            std::string_view(line.data() + separator + 1, line.size() - separator - 1)
        );
        const std::vector<std::uint8_t> encoded = hyprfen::encode_fen(fen);
        if (hyprfen::decode_fen(expected) != fen || encoded != expected ||
            hyprfen::decode_fen(encoded) != fen) {
            throw std::runtime_error("bitstream compatibility mismatch at line " +
                                     std::to_string(line_number));
        }
        ++cases;
    }
    expect(cases == 100'000, "unexpected bitstream vector count");
}

}  // namespace

int main(int argc, char* argv[]) {
    const GoldenCase golden_cases[] = {
        {
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            "ffff00000000ffffffff000000ffff0042429294591f4000",
            24,
        },
        {
            "r1bqk2r/pppp1ppp/5n2/n1b1p1B1/2B1P3/2NP1Q2/PPP2PPP/R3K1NR b KQkq - 4 6",
            "d1e72c145520ef9dff7f0400f04bf20794047051691e8101",
            24,
        },
        {
            "rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 1",
            "ffef00001800f7ffff7f010000ffff004242929459ff000200",
            25,
        },
        {
            "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 17 42",
            "005000a283080400672c0d9004112a",
            15,
        },
        {
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
            "91ffa41218737d91ff9f030038e7d10ba00658516a1f4000",
            24,
        },
    };

    for (const GoldenCase& test_case : golden_cases) {
        const std::vector<std::uint8_t> encoded = hyprfen::encode_fen(test_case.fen);
        expect(encoded.size() == test_case.bytes, "unexpected encoded byte length");
        expect(to_hex(encoded) == test_case.hex, "encoded bytes do not match Python reference");
        expect(hyprfen::decode_fen(encoded) == test_case.fen, "round-trip decode mismatch");
        const hyprfen::Components components = hyprfen::decode_components(encoded);
        expect(
            hyprfen::decode_fen(hyprfen::encode_components(components)) == test_case.fen,
            "component round-trip mismatch"
        );
        const hyprfen::EncodeStats stats = hyprfen::encoding_stats(test_case.fen);
        expect(stats.encoded_bytes == encoded.size(), "encoding_stats encoded_bytes mismatch");
        expect(stats.encoded_bits >= encoded.size() * 8 - 7, "encoding_stats encoded_bits mismatch");
    }

    expect_throws<hyprfen::Error>(
        [] {
            static_cast<void>(hyprfen::encode_fen("8/8/8/8/8/8/8/8 w - - 0"));
        },
        "invalid FEN should throw"
    );

    expect_throws<hyprfen::MalformedEncodingError>(
        [] {
            const std::vector<std::uint8_t> encoded = hyprfen::encode_fen(
                "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
            );
            std::vector<std::uint8_t> malformed = encoded;
            malformed.push_back(0x00);
            static_cast<void>(hyprfen::decode_fen(malformed));
        },
        "extra zero byte should be rejected"
    );

    expect_throws<hyprfen::MalformedEncodingError>(
        [] {
            const std::vector<std::uint8_t> encoded = hyprfen::encode_fen(
                "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 17 42"
            );
            std::vector<std::uint8_t> malformed = encoded;
            malformed.back() = static_cast<std::uint8_t>(malformed.back() | 0x80u);
            static_cast<void>(hyprfen::decode_fen(malformed));
        },
        "non-zero padding bits should be rejected"
    );

    if (argc == 2) {
        test_bitstream_vectors(argv[1]);
    } else {
        expect(argc == 1, "expected at most one bitstream vector path");
    }

    std::cout << "hyprfen_tests passed\n";
    return 0;
}
