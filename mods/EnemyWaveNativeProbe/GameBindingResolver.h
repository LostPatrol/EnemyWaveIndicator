// Resolve audited bindings from a mapped PE image without executing or changing game code.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace ewi::bindings {
// Fixed storage bounds keep discovery allocation-free inside the game process.
inline constexpr size_t MaxPatterns = 16, MaxSections = 96;
// FNV-1a parameters are shared with the offline profile generator.
inline constexpr uint64_t FnvOffset = 14695981039346656037ull, FnvPrime = 1099511628211ull;
// PE section permission flags used to distinguish code from thread-id storage.
inline constexpr uint32_t Executable = 0x20000000, Readable = 0x40000000;
struct MaskRange { uint32_t offset; uint8_t size; };
struct Pattern { const char* name; uint32_t size; uint64_t normalizedHash; const char* anchor;
    const MaskRange* masks; size_t maskCount; bool functionStart; };
struct Edge { const char* name; uint8_t owner; uint32_t returnOffset; uint8_t target; uint32_t targetOffset; };
struct Window { const char* name; uint8_t owner; uint32_t offset; const char* bytes; };
struct Profile { const Pattern* patterns; size_t patternCount; const Edge* edges; size_t edgeCount;
    const Window* windows; size_t windowCount; uint8_t threadPattern; };
struct Section { uint32_t rva = 0, size = 0, flags = 0; };
struct Image {
    const unsigned char* base = nullptr; size_t size = 0;
    std::array<Section, MaxSections> sections{}; size_t sectionCount = 0;
    uint32_t imports = 0, importSize = 0, pdata = 0, pdataSize = 0;
};
struct Resolution {
    bool ok = false; const char* error = nullptr; const char* failedName = nullptr;
    uint32_t failedPattern = UINT32_MAX, candidates = 0;
    std::array<uint32_t, MaxPatterns> rvas{}, returns{}, candidateCounts{}; uint32_t threadIdRva = 0;
    std::array<std::array<unsigned char, 24>, MaxPatterns> entryBytes{}; // Discovery-time bytes must never follow later patches.
};

// Check subtraction-based bounds before every image read.
inline bool range(size_t at, size_t count, size_t size) noexcept { return at <= size && count <= size - at; }
// Read unaligned PE fields without relying on compiler packing.
template<class T> inline bool read(const Image& image, size_t at, T& value) noexcept {
    if (!range(at, sizeof(T), image.size)) return false;
    std::memcpy(&value, image.base + at, sizeof(T)); return true;
}
// Require the complete span to belong to one mapped section with the requested permissions.
inline bool sectionSpan(const Image& image, uint32_t at, size_t count, uint32_t required, uint32_t forbidden = 0) noexcept {
    if (!range(at, count, image.size)) return false;
    for (size_t i = 0; i < image.sectionCount; ++i) {
        const auto& s = image.sections[i];
        if (at >= s.rva && range(static_cast<size_t>(at - s.rva), count, s.size)
            && (s.flags & required) == required && !(s.flags & forbidden)) return true;
    }
    return false;
}
// Parse only PE32+ metadata; the caller owns the mapped bytes for the entire resolution.
inline bool openImage(const void* base, size_t available, Image& image) noexcept {
    image = {}; if (!base) return false;
    Image parsed; parsed.base = static_cast<const unsigned char*>(base); parsed.size = available;
    uint16_t mz = 0, magic = 0, sectionCount = 0, optionalSize = 0, machine = 0;
    uint32_t nt = 0, signature = 0, imageSize = 0, directories = 0;
    if (!read(parsed, 0, mz) || mz != 0x5a4d || !read(parsed, 0x3c, nt)
        || !range(nt, 24, available) || !read(parsed, nt, signature) || signature != 0x4550
        || !read(parsed, static_cast<size_t>(nt) + 4, machine) || machine != 0x8664
        || !read(parsed, static_cast<size_t>(nt) + 6, sectionCount) || !sectionCount || sectionCount > MaxSections
        || !read(parsed, static_cast<size_t>(nt) + 20, optionalSize) || optionalSize < 144) return false;
    const size_t opt = static_cast<size_t>(nt) + 24;
    if (!range(opt, optionalSize, available) || !read(parsed, opt, magic) || magic != 0x20b
        || !read(parsed, opt + 56, imageSize) || !imageSize || imageSize > available
        || !read(parsed, opt + 108, directories) || directories < 4) return false;
    parsed.size = imageSize;
    if (!read(parsed, opt + 120, parsed.imports) || !read(parsed, opt + 124, parsed.importSize)
        || !read(parsed, opt + 136, parsed.pdata) || !read(parsed, opt + 140, parsed.pdataSize)
        || !range(parsed.imports, parsed.importSize, parsed.size) || !range(parsed.pdata, parsed.pdataSize, parsed.size)
        || parsed.pdataSize % 12) return false;
    const size_t table = opt + optionalSize;
    if (!range(table, static_cast<size_t>(sectionCount) * 40, parsed.size)) return false;
    parsed.sectionCount = sectionCount;
    for (size_t i = 0; i < sectionCount; ++i) {
        auto& s = parsed.sections[i]; const size_t at = table + i * 40;
        if (!read(parsed, at + 8, s.size) || !read(parsed, at + 12, s.rva) || !read(parsed, at + 36, s.flags)
            || !range(s.rva, s.size, parsed.size)) return false;
        for (size_t j = 0; j < i; ++j) {
            const auto& other = parsed.sections[j];
            if (s.size && other.size && s.rva < static_cast<uint64_t>(other.rva) + other.size
                && other.rva < static_cast<uint64_t>(s.rva) + s.size) return false;
        }
    }
    image = parsed; return true;
}
// Decode hexadecimal anchors; wildcards are permitted only as whole bytes.
inline int hex(char ch) noexcept {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}
struct Bytes { std::array<unsigned char, 64> values{}, fixed{}; size_t size = 0; };
// Reject malformed, empty and all-wildcard anchors before scanning.
inline bool parseBytes(const char* text, Bytes& out, bool wildcards) noexcept {
    out = {}; if (!text) return false; bool anyFixed = false;
    for (size_t at = 0; text[at]; at += 2) {
        if (out.size == out.values.size() || !text[at + 1]) return false;
        const bool wild = text[at] == '?' && text[at + 1] == '?';
        const int a = hex(text[at]), b = hex(text[at + 1]);
        if ((!wild && (a < 0 || b < 0)) || (wild && !wildcards)) return false;
        out.fixed[out.size] = wild ? 0 : 1;
        out.values[out.size++] = wild ? 0 : static_cast<unsigned char>(a * 16 + b);
        anyFixed = anyFixed || !wild;
    }
    return out.size && anyFixed;
}
// Compare an anchor only after the containing image span has been checked.
inline bool matches(const Image& image, uint32_t rva, const Bytes& bytes) noexcept {
    if (!range(rva, bytes.size, image.size)) return false;
    for (size_t i = 0; i < bytes.size; ++i)
        if (bytes.fixed[i] && image.base[rva + i] != bytes.values[i]) return false;
    return true;
}
// Validate masks once; a mask may never hide bytes beyond its audited function span.
inline bool validMasks(const Pattern& pattern) noexcept {
    if (pattern.maskCount && !pattern.masks) return false;
    uint64_t previousEnd = 0;
    for (size_t i = 0; i < pattern.maskCount; ++i) {
        const auto& m = pattern.masks[i];
        const uint64_t end = static_cast<uint64_t>(m.offset) + m.size;
        if (!m.size || m.offset < previousEnd || end > pattern.size) return false;
        previousEnd = end;
    }
    return true;
}
// Hash complete audited spans while normalizing only declared relocation operands.
inline uint64_t normalizedHash(const unsigned char* bytes, const Pattern& pattern) noexcept {
    uint64_t hash = FnvOffset; size_t mask = 0;
    for (uint32_t i = 0; i < pattern.size; ++i) {
        while (mask < pattern.maskCount && i >= static_cast<uint64_t>(pattern.masks[mask].offset) + pattern.masks[mask].size) ++mask;
        const bool hidden = mask < pattern.maskCount && i >= pattern.masks[mask].offset;
        hash = (hash ^ (hidden ? 0 : bytes[i])) * FnvPrime;
    }
    return hash;
}
// Resolve a signed relative address without unsigned wrapping.
inline bool relative(uint32_t end, int32_t displacement, uint32_t& target) noexcept {
    const int64_t result = static_cast<int64_t>(end) + displacement;
    if (result < 0 || result > UINT32_MAX) return false;
    target = static_cast<uint32_t>(result); return true;
}
// Compare a bounded import name, including its terminating zero.
inline bool named(const Image& image, uint32_t rva, const char* expected) noexcept {
    const size_t count = std::strlen(expected) + 1;
    return sectionSpan(image, rva, count, Readable) && !std::memcmp(image.base + rva, expected, count);
}
// Match the selected IAT slot to a named import using OriginalFirstThunk, never live IAT contents.
inline bool threadImport(const Image& image, uint32_t slot) noexcept {
    for (size_t at = image.imports; range(at - image.imports, 20, image.importSize); at += 20) {
        uint32_t original = 0, name = 0, first = 0;
        if (!read(image, at, original) || !read(image, at + 12, name) || !read(image, at + 16, first)) return false;
        if (!original && !name && !first) break;
        if (!original || slot < first || (slot - first) % 8) continue;
        // Walk from the beginning so a slot past an earlier terminator cannot masquerade as an import.
        const uint32_t wanted = (slot - first) / 8;
        for (uint64_t index = 0; index <= wanted; ++index) {
            const uint64_t lookup = static_cast<uint64_t>(original) + index * 8;
            uint64_t thunk = 0;
            if (lookup > UINT32_MAX || !sectionSpan(image, static_cast<uint32_t>(lookup), 8, Readable)
                || !read(image, static_cast<size_t>(lookup), thunk) || !thunk) break;
            if (index == wanted && !(thunk >> 63) && thunk <= UINT32_MAX - 2
                && named(image, static_cast<uint32_t>(thunk) + 2, "GetCurrentThreadId")) return true;
        }
    }
    return false;
}
// Check public view/profile bounds before discovery or fixed-address revalidation.
inline bool validInput(const Image& image, const Profile& profile) noexcept {
    return image.base && image.size && image.size <= UINT32_MAX && image.sectionCount && image.sectionCount <= MaxSections
        && range(image.pdata, image.pdataSize, image.size) && !(image.pdataSize % 12)
        && range(image.imports, image.importSize, image.size)
        && profile.patterns && profile.patternCount && profile.patternCount <= MaxPatterns
        && profile.edgeCount <= MaxPatterns && (!profile.edgeCount || profile.edges) && (!profile.windowCount || profile.windows)
        && profile.threadPattern < profile.patternCount;
}
// Recheck only discovered addresses; never refresh their entry snapshots or search for replacements.
inline bool validateResolved(const Image& image, const Profile& profile, Resolution& result) noexcept;
// Find unique audited patterns and prove all call, layout and thread-identity relationships.
inline bool resolve(const Image& image, const Profile& profile, Resolution& result) noexcept {
    result = {};
    auto fail = [&result](const char* error, const char* name) { result.ok = false; result.error = error; result.failedName = name; return false; };
    if (!validInput(image, profile)) return fail("Invalid profile/image", nullptr);
    size_t firstBad = profile.patternCount;
    for (size_t i = 0; i < profile.patternCount; ++i) {
        result.failedPattern = static_cast<uint32_t>(i); result.candidates = 0;
        const auto& p = profile.patterns[i]; Bytes anchor;
        if (!p.size || !validMasks(p) || !parseBytes(p.anchor, anchor, true) || anchor.size > p.size)
            return fail("Invalid pattern/masks", p.name);
        auto candidate = [&](uint32_t rva) {
            if (!matches(image, rva, anchor) || !sectionSpan(image, rva, p.size, Executable)
                || normalizedHash(image.base + rva, p) != p.normalizedHash) return false;
            result.rvas[i] = rva;
            const size_t count = p.size < result.entryBytes[i].size() ? p.size : result.entryBytes[i].size();
            std::memcpy(result.entryBytes[i].data(), image.base + rva, count);
            return true;
        };
        if (p.functionStart) {
            for (size_t at = image.pdata; at < static_cast<size_t>(image.pdata) + image.pdataSize; at += 12) {
                uint32_t begin = 0, end = 0;
                if (!read(image, at, begin) || !read(image, at + 4, end) || end <= begin
                    || !sectionSpan(image, begin, end - begin, Executable) || !candidate(begin)) continue;
                // Audited spans may cross unwind chunks; only the entry's first chunk must start here.
                bool duplicate = false;
                for (size_t previous = image.pdata; previous < at; previous += 12) {
                    uint32_t other = 0, otherEnd = 0;
                    if (read(image, previous, other) && read(image, previous + 4, otherEnd) && other == begin
                        && otherEnd > other && sectionSpan(image, other, otherEnd - other, Executable)) { duplicate = true; break; }
                }
                if (!duplicate) ++result.candidates;
            }
        } else {
            for (size_t s = 0; s < image.sectionCount; ++s) {
                const auto& section = image.sections[s]; if (!(section.flags & Executable) || p.size > section.size) continue;
                for (uint64_t at = section.rva; at <= static_cast<uint64_t>(section.rva) + section.size - p.size; ++at)
                    if (candidate(static_cast<uint32_t>(at))) ++result.candidates;
            }
        }
        result.candidateCounts[i] = result.candidates;
        if (result.candidates != 1 && firstBad == profile.patternCount) firstBad = i;
    }
    if (firstBad != profile.patternCount) {
        result.failedPattern = static_cast<uint32_t>(firstBad); result.candidates = result.candidateCounts[firstBad];
        return fail(result.candidates ? "Ambiguous pattern" : "Pattern not found", profile.patterns[firstBad].name);
    }
    return validateResolved(image, profile, result);
}
// Validate full spans, unchanged captured entries and their semantic relationships before hook installation.
inline bool validateResolved(const Image& image, const Profile& profile, Resolution& result) noexcept {
    result.ok = false; result.error = result.failedName = nullptr;
    result.failedPattern = UINT32_MAX; result.candidates = 0;
    auto fail = [&result](const char* error, const char* name) { result.error = error; result.failedName = name; return false; };
    if (!validInput(image, profile)) return fail("Invalid profile/image", nullptr);
    for (size_t i = 0; i < profile.patternCount; ++i) {
        const auto& p = profile.patterns[i]; const uint32_t rva = result.rvas[i]; Bytes anchor;
        result.failedPattern = static_cast<uint32_t>(i); result.candidates = result.candidateCounts[i];
        if (!p.size || !validMasks(p) || !parseBytes(p.anchor, anchor, true) || anchor.size > p.size)
            return fail("Invalid pattern/masks", p.name);
        if (result.candidateCounts[i] != 1) return fail("Unresolved pattern", p.name);
        if (!sectionSpan(image, rva, p.size, Executable) || !matches(image, rva, anchor)
            || normalizedHash(image.base + rva, p) != p.normalizedHash) return fail("Resolved pattern changed", p.name);
        const size_t count = p.size < result.entryBytes[i].size() ? p.size : result.entryBytes[i].size();
        if (std::memcmp(result.entryBytes[i].data(), image.base + rva, count)) return fail("Resolved entry changed", p.name);
    }
    result.failedPattern = UINT32_MAX; result.candidates = 0;
    for (size_t i = 0; i < profile.edgeCount; ++i) {
        const auto& e = profile.edges[i];
        if (e.owner >= profile.patternCount || e.target >= profile.patternCount || e.returnOffset < 5
            || e.returnOffset > profile.patterns[e.owner].size || e.targetOffset >= profile.patterns[e.target].size)
            return fail("Invalid edge", e.name);
        const uint64_t end64 = static_cast<uint64_t>(result.rvas[e.owner]) + e.returnOffset;
        if (end64 > UINT32_MAX) return fail("Edge overflow", e.name);
        const auto end = static_cast<uint32_t>(end64); int32_t displacement = 0; uint32_t target = 0;
        if (!sectionSpan(image, end - 5, 5, Executable) || image.base[end - 5] != 0xe8 || !read(image, end - 4, displacement)
            || !relative(end, displacement, target) || target != static_cast<uint64_t>(result.rvas[e.target]) + e.targetOffset)
            return fail("Call edge mismatch", e.name);
        result.returns[i] = end;
    }
    for (size_t i = 0; i < profile.windowCount; ++i) {
        const auto& w = profile.windows[i]; Bytes bytes;
        if (w.owner >= profile.patternCount || !parseBytes(w.bytes, bytes, false)
            || !range(w.offset, bytes.size, profile.patterns[w.owner].size)) return fail("Invalid window", w.name);
        if (!matches(image, result.rvas[w.owner] + w.offset, bytes)) return fail("ABI window mismatch", w.name);
    }
    const auto& p = profile.patterns[profile.threadPattern]; const uint32_t at = result.rvas[profile.threadPattern];
    if (p.functionStart || p.size != 22 || !sectionSpan(image, at, 22, Executable)
        || image.base[at] != 0xff || image.base[at + 1] != 0x15
        || image.base[at + 9] != 0x44 || image.base[at + 10] != 0x88 || image.base[at + 11] != 0x35
        || image.base[at + 16] != 0x89 || image.base[at + 17] != 0x05) return fail("Thread instruction mismatch", p.name);
    int32_t importDisp = 0, flagDisp = 0, threadDisp = 0; uint32_t slot = 0, flag = 0, thread = 0;
    if (!read(image, at + 2, importDisp) || !read(image, at + 12, flagDisp) || !read(image, at + 18, threadDisp)
        || !relative(at + 6, importDisp, slot) || !relative(at + 16, flagDisp, flag) || !relative(at + 22, threadDisp, thread)
        || static_cast<uint64_t>(thread) + 16 != flag || !sectionSpan(image, thread, 17, Readable, Executable)
        || !sectionSpan(image, slot, 8, Readable) || !threadImport(image, slot)) return fail("Thread globals/import mismatch", p.name);
    result.threadIdRva = thread; result.ok = true; return true;
}
} // namespace ewi::bindings
