#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "util.h"

namespace lang {
namespace util {

std::string g_username;

static std::uint8_t g_fast_rng_state;

static unsigned char hexdecode_nibble(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'A' && ch <= 'F')
        return 10 + (ch - 'A');
    if (ch >= 'a' && ch <= 'f')
        return 10 + (ch - 'a');
    throw std::runtime_error("invalid hex digit");
}

std::string hexdecode(const char *s)
{
    if (strlen(s) % 2)
        throw std::runtime_error("invalid hex string length");

    std::string output;
    for (; *s; s += 2) {
        auto hi = hexdecode_nibble(s[0]);
        auto lo = hexdecode_nibble(s[1]);
        output.push_back((hi << 4) | lo);
    }

    return output;
}

void fast_rng_seed(std::uint8_t seed) {
    g_fast_rng_state = seed;
}

std::uint8_t fast_rng_rand() {
    g_fast_rng_state += 1;
    return g_fast_rng_state;
}

std::int64_t str_to_int(std::string_view str, std::size_t *endp) {
    bool neg = str.starts_with('-');
    auto str_noneg = str.substr(neg ? 1 : 0);

    int base;
    std::size_t start;
    const char *charset;
    if (str_noneg.starts_with("&O")) {
        base = 8;
        start = 2;
        charset = "01234567";
    } else if (str_noneg.starts_with("&H")) {
        base = 16;
        start = 2;
        charset = "0123456789ABCDEFabcdef";
    } else {
        base = 10;
        start = 0;
        charset = "0123456789";
    }

    if (neg)
        start += 1;

    auto end = str.find_first_not_of(charset, start);
    if (end == std::string_view::npos)
        end = str.size();
    if (end == start) {
        if (endp)
            *endp = 0;
        return 0;
    }
    if (endp)
        *endp = end;

    std::string digits(str.substr(start, end - start));
    if (neg)
        digits = '-' + digits;

    return std::stoll(digits, nullptr, base);
}

std::string read_file(const std::string &path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("cannot open file \"" + path + "\"");
    std::stringstream content;
    content << file.rdbuf();
    return content.str();
}

} // namespace util
} // namespace lang
