// Map an EXE into ordinary byte vectors and report the production resolver's decisions as JSON.
#include "../mods/EnemyWaveNativeProbe/GameBindingProfile.generated.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace ewi::bindings;
// Bound malformed image allocations without restricting the game's current PE image size.
constexpr uint32_t MaxImageSize = 1024u * 1024u * 1024u;
// Escape every JSON control character, including diagnostic file paths.
std::string json(const char* text) {
    if (!text) return "null";
    std::ostringstream out; out << '"';
    for (const unsigned char* at = reinterpret_cast<const unsigned char*>(text); *at; ++at) {
        if (*at == '"' || *at == '\\') out << '\\' << static_cast<char>(*at);
        else if (*at < 0x20) {
            const char digits[] = "0123456789abcdef"; // JSON control characters use two low hexadecimal digits.
            out << "\\u00" << digits[*at >> 4] << digits[*at & 15];
        } else out << static_cast<char>(*at);
    }
    out << '"'; return out.str();
}
// Read disk metadata with explicit bounds before using any length or RVA.
template<class T> T field(const std::vector<unsigned char>& bytes, size_t at) {
    if (!range(at, sizeof(T), bytes.size())) throw std::runtime_error("Truncated PE field");
    T value{}; std::memcpy(&value, bytes.data() + at, sizeof(value)); return value;
}
// Recreate section placement in non-executable vector storage, without loading a Windows module.
std::vector<unsigned char> mapFile(const char* path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error(std::string("Cannot open file: ") + path);
    const auto length = input.tellg();
    if (length <= 0 || static_cast<uint64_t>(length) > MaxImageSize) throw std::runtime_error("Invalid/oversized PE file");
    std::vector<unsigned char> disk(static_cast<size_t>(length)); input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(disk.data()), static_cast<std::streamsize>(disk.size())))
        throw std::runtime_error("Could not read complete PE file");
    if (field<uint16_t>(disk, 0) != 0x5a4d) throw std::runtime_error("Missing MZ header");
    const size_t nt = field<uint32_t>(disk, 0x3c);
    if (!range(nt, 24, disk.size()) || field<uint32_t>(disk, nt) != 0x4550
        || field<uint16_t>(disk, nt + 4) != 0x8664) throw std::runtime_error("Expected x64 PE image");
    const uint16_t count = field<uint16_t>(disk, nt + 6), optionalSize = field<uint16_t>(disk, nt + 20);
    const size_t opt = nt + 24;
    if (!count || count > MaxSections || optionalSize < 144 || !range(opt, optionalSize, disk.size())
        || field<uint16_t>(disk, opt) != 0x20b) throw std::runtime_error("Invalid PE32+ headers");
    const uint32_t size = field<uint32_t>(disk, opt + 56), headers = field<uint32_t>(disk, opt + 60);
    const size_t table = opt + optionalSize;
    if (!size || size > MaxImageSize || !headers || headers > size || headers > disk.size()
        || !range(table, static_cast<size_t>(count) * 40, headers)) throw std::runtime_error("Invalid image/header size");
    std::vector<unsigned char> mapped(size); std::memcpy(mapped.data(), disk.data(), headers);
    for (size_t i = 0; i < count; ++i) {
        const size_t at = table + i * 40;
        const uint32_t virtualSize = field<uint32_t>(disk, at + 8), rva = field<uint32_t>(disk, at + 12);
        const uint32_t rawSize = field<uint32_t>(disk, at + 16), raw = field<uint32_t>(disk, at + 20);
        if (!range(rva, virtualSize, mapped.size()) || !range(rva, rawSize, mapped.size())
            || !range(raw, rawSize, disk.size()) || (rawSize && rva < headers)) throw std::runtime_error("Invalid section span");
        if (rawSize) std::memcpy(mapped.data() + rva, disk.data() + raw, rawSize);
    }
    return mapped;
}
// Print a complete record even when discovery or PE parsing fails.
void report(const Resolution& r, const std::string& parsingError, double elapsedMs) {
    const char* error = parsingError.empty() ? r.error : parsingError.c_str();
    std::cout << "{\"passed\":" << (r.ok && parsingError.empty() ? "true" : "false")
        << ",\"error\":" << json(error) << ",\"failedName\":" << json(r.failedName)
        << ",\"failedPattern\":" << r.failedPattern << ",\"candidates\":" << r.candidates
        << ",\"elapsedMs\":" << elapsedMs << ",\"rvas\":{";
    for (size_t i = 0; i < profile::Patterns.size(); ++i) {
        if (i) std::cout << ',';
        std::cout << json(profile::Patterns[i].name) << ':' << r.rvas[i];
    }
    std::cout << "},\"candidateCounts\":{";
    for (size_t i = 0; i < profile::Patterns.size(); ++i) {
        if (i) std::cout << ',';
        std::cout << json(profile::Patterns[i].name) << ':' << r.candidateCounts[i];
    }
    std::cout << "},\"returns\":{";
    for (size_t i = 0; i < profile::Edges.size(); ++i) {
        if (i) std::cout << ',';
        std::cout << json(profile::Edges[i].name) << ':' << r.returns[i];
    }
    std::cout << "},\"threadIdRva\":" << r.threadIdRva << ",\"profileSha256\":" << json(profile::Sha256) << "}\n";
}
} // namespace

// Never execute image bytes: resolution sees only the explicitly mapped vector's memory.
int main(int argc, char** argv) {
    const auto started = std::chrono::steady_clock::now();
    Resolution result; std::string parsingError; int status = 2;
    try {
        if (argc != 2) throw std::runtime_error("Usage: game-bindings-inspect <game-exe>");
        const auto mapped = mapFile(argv[1]); Image image;
        if (!openImage(mapped.data(), mapped.size(), image)) throw std::runtime_error("Mapped PE metadata rejected");
        status = resolve(image, profile::Data, result) ? 0 : 1;
    } catch (const std::exception& error) { result.ok = false; parsingError = error.what(); }
    const double elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    report(result, parsingError, elapsed); return status;
}
