#ifndef UTIL_H
#define UTIL_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace lang {
namespace util {

extern std::string g_username;

std::string hexdecode(const char *s);

void fast_rng_seed(std::uint8_t seed);
std::uint8_t fast_rng_rand();

std::int64_t str_to_int(std::string_view str, std::size_t *endp = nullptr);

std::string read_file(const std::string &path);

} // namespace util
} // namespace lang

#endif // UTIL_H
