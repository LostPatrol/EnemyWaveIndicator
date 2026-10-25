// Exercise mapped-PE discovery and reject ambiguous or structurally incompatible bindings in memory.
#include "../mods/EnemyWaveNativeProbe/GameBindingResolver.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
using namespace ewi::bindings;
// The fixture has separate executable and readable data sections, with PE import and pdata tables.
constexpr uint32_t Code = 0x1000, Data = 0x3000, Pdata = 0x3300, Imports = 0x3400;
constexpr uint32_t Lookup = 0x3500, Iat = 0x3600, ImportName = 0x3700, Thread = 0x3100;
// Abort on the first failed invariant so build integration cannot report a partial success.
void require(bool value, int line) {
    if (!value) { std::printf("FAIL: native-game-bindings line %d\n", line); std::exit(1); }
}
#define REQUIRE(value) require((value), __LINE__)
struct Fixture {
    std::vector<unsigned char> memory = std::vector<unsigned char>(0x5000);
    std::array<uint32_t, 3> positions{};
    std::array<MaskRange, 1> callMasks{{{5, 4}}};
    std::array<MaskRange, 3> threadMasks{{{2, 4}, {12, 4}, {18, 4}}};
    std::array<Pattern, 3> patterns{};
    std::array<Edge, 1> edges{{{"caller-to-target", 0, 9, 1, 0}}};
    std::array<Window, 1> windows{{{"queue-layout", 0, 9, "488b4128"}}};
    Profile profile{}; Image image{};
    // Write scalar fixture metadata without alignment or aliasing assumptions.
    template<class T> void put(size_t at, T value) { std::memcpy(memory.data() + at, &value, sizeof(value)); }
    // Write a signed relative operand after proving the small fixture arithmetic explicitly.
    void displacement(uint32_t at, uint32_t end, uint32_t target) {
        put<int32_t>(at, static_cast<int32_t>(target) - static_cast<int32_t>(end));
    }
    // Set up one complete valid profile; shifts change code addresses without changing normalized hashes.
    explicit Fixture(uint32_t shift = 0) {
        positions = {0x1100 + shift, 0x1200 + shift, 0x1300 + shift};
        put<uint16_t>(0, 0x5a4d); put<uint32_t>(0x3c, 0x80); put<uint32_t>(0x80, 0x4550);
        put<uint16_t>(0x84, 0x8664); put<uint16_t>(0x86, 2); put<uint16_t>(0x94, 0xf0);
        const uint32_t opt = 0x98; // Minimal PE32+ optional header in the synthetic image.
        put<uint16_t>(opt, 0x20b); put<uint32_t>(opt + 56, static_cast<uint32_t>(memory.size()));
        put<uint32_t>(opt + 108, 16); put<uint32_t>(opt + 120, Imports); put<uint32_t>(opt + 124, 40);
        put<uint32_t>(opt + 136, Pdata); put<uint32_t>(opt + 140, 24);
        const uint32_t table = opt + 0xf0; // Section table follows the declared optional header.
        put<uint32_t>(table + 8, 0x1000); put<uint32_t>(table + 12, Code); put<uint32_t>(table + 36, Executable | Readable);
        put<uint32_t>(table + 48, 0x1000); put<uint32_t>(table + 52, Data); put<uint32_t>(table + 76, Readable);
        const unsigned char caller[] = {0x48,0x83,0xec,0x28,0xe8,0,0,0,0,0x48,0x8b,0x41,0x28,0xc3};
        const unsigned char target[] = {0x40,0x53,0x48,0x83,0xec,0x20,0x90,0xc3};
        const unsigned char identity[] = {0xff,0x15,0,0,0,0,0x49,0x8b,0xcf,0x44,0x88,0x35,0,0,0,0,0x89,0x05,0,0,0,0};
        std::memcpy(memory.data() + positions[0], caller, sizeof(caller));
        std::memcpy(memory.data() + positions[1], target, sizeof(target));
        std::memcpy(memory.data() + positions[2], identity, sizeof(identity));
        displacement(positions[0] + 5, positions[0] + 9, positions[1]);
        displacement(positions[2] + 2, positions[2] + 6, Iat);
        displacement(positions[2] + 12, positions[2] + 16, Thread + 16);
        displacement(positions[2] + 18, positions[2] + 22, Thread);
        for (size_t i = 0; i < 2; ++i) {
            put<uint32_t>(Pdata + i * 12, positions[i]); put<uint32_t>(Pdata + i * 12 + 4, positions[i] + 32);
        }
        put<uint32_t>(Imports, Lookup); put<uint32_t>(Imports + 12, ImportName + 64); put<uint32_t>(Imports + 16, Iat);
        put<uint64_t>(Lookup, ImportName); put<uint64_t>(Iat, 0x7ff012345678ull);
        std::memcpy(memory.data() + ImportName + 2, "GetCurrentThreadId", 19);
        std::memcpy(memory.data() + ImportName + 64, "KERNEL32.dll", 13);
        patterns[0] = {"caller", static_cast<uint32_t>(sizeof(caller)), 0, "4883ec28e8????????488b4128c3", callMasks.data(), callMasks.size(), true};
        patterns[1] = {"target", static_cast<uint32_t>(sizeof(target)), 0, "40534883", nullptr, 0, true};
        patterns[2] = {"thread", static_cast<uint32_t>(sizeof(identity)), 0, "ff15????????498bcf448835????????8905????????", threadMasks.data(), threadMasks.size(), false};
        for (size_t i = 0; i < patterns.size(); ++i) patterns[i].normalizedHash = normalizedHash(memory.data() + positions[i], patterns[i]);
        profile = {patterns.data(), patterns.size(), edges.data(), edges.size(), windows.data(), windows.size(), 2};
        REQUIRE(openImage(memory.data(), memory.size(), image));
    }
};

// Prove valid relocation, full-span hashing, call edges, pdata and named-import thread identity.
void validRelocation() {
    Fixture original, relocated(0x100); Resolution a, b;
    REQUIRE(resolve(original.image, original.profile, a) && a.ok);
    REQUIRE(resolve(relocated.image, relocated.profile, b) && b.ok);
    REQUIRE(a.rvas[0] == 0x1100 && b.rvas[0] == 0x1200 && b.returns[0] == 0x1209);
    REQUIRE(a.threadIdRva == Thread && b.threadIdRva == Thread);
    REQUIRE(original.patterns[0].normalizedHash == relocated.patterns[0].normalizedHash);
    REQUIRE(original.patterns[2].normalizedHash == relocated.patterns[2].normalizedHash);
    const Pattern hello{"known-fnv", 5, 0, "68656c6c6f", nullptr, 0, false};
    REQUIRE(normalizedHash(reinterpret_cast<const unsigned char*>("hello"), hello) == 0xa430d84680aabd0bull);
}
// Reject duplicate valid starts while ignoring identical bytes in the middle of an unrelated function.
void candidateIdentity() {
    Fixture f; Resolution r;
    f.put<uint32_t>(Pdata + 4, f.positions[0] + 4);
    REQUIRE(resolve(f.image, f.profile, r)); // Full-span validation may extend beyond the first unwind chunk.
    std::memcpy(f.memory.data() + 0x1500, f.memory.data() + f.positions[1], f.patterns[1].size);
    REQUIRE(resolve(f.image, f.profile, r));
    f.put<uint32_t>(Pdata + 24, f.positions[1]); f.put<uint32_t>(Pdata + 28, f.positions[1] + 32);
    f.put<uint32_t>(0x98 + 140, 36); REQUIRE(openImage(f.memory.data(), f.memory.size(), f.image));
    REQUIRE(resolve(f.image, f.profile, r) && r.candidateCounts[1] == 1);
    f.put<uint32_t>(Pdata + 24, 0x1500); f.put<uint32_t>(Pdata + 28, 0x1520);
    REQUIRE(!resolve(f.image, f.profile, r) && !r.ok && r.candidates == 2 && r.failedPattern == 1);
    REQUIRE(r.candidateCounts[0] == 1 && r.candidateCounts[1] == 2 && r.candidateCounts[2] == 1);
    REQUIRE(!std::strcmp(r.error, "Ambiguous pattern"));
}
// Masked operand changes must still satisfy semantic call/import/layout relationships.
void semanticRejections() {
    Fixture f; Resolution r;
    f.displacement(f.positions[0] + 5, f.positions[0] + 9, f.positions[1] + 1);
    REQUIRE(!resolve(f.image, f.profile, r) && !r.ok && !std::strcmp(r.failedName, "caller-to-target"));
    f.displacement(f.positions[0] + 5, f.positions[0] + 9, f.positions[1]);
    f.windows[0].bytes = "488b4130";
    REQUIRE(!resolve(f.image, f.profile, r) && !std::strcmp(r.failedName, "queue-layout"));
    f.windows[0].bytes = "488b4128"; f.memory[ImportName + 2] = 'X';
    REQUIRE(!resolve(f.image, f.profile, r) && !std::strcmp(r.error, "Thread globals/import mismatch"));
    f.memory[ImportName + 2] = 'G'; f.displacement(f.positions[2] + 18, f.positions[2] + 22, Code);
    f.displacement(f.positions[2] + 12, f.positions[2] + 16, Code + 16);
    REQUIRE(!resolve(f.image, f.profile, r) && !r.ok);
    Fixture terminated;
    terminated.put<uint64_t>(Lookup, 0); terminated.put<uint64_t>(Lookup + 8, ImportName);
    terminated.displacement(terminated.positions[2] + 2, terminated.positions[2] + 6, Iat + 8);
    REQUIRE(!resolve(terminated.image, terminated.profile, r) && !r.ok);
}
// Reject malformed metadata, invalid masks and unmasked code changes without reading outside the image.
void malformedInputs() {
    Fixture f; Resolution r; Image invalid;
    REQUIRE(!openImage(f.memory.data(), 100, invalid));
    f.put<uint32_t>(0x98 + 56, UINT32_MAX); REQUIRE(!openImage(f.memory.data(), f.memory.size(), invalid));
    f.put<uint32_t>(0x98 + 56, static_cast<uint32_t>(f.memory.size()));
    f.callMasks[0] = {13, 4}; REQUIRE(!resolve(f.image, f.profile, r) && !r.ok);
    f.callMasks[0] = {5, 4}; f.threadMasks[1] = {4, 4};
    REQUIRE(!resolve(f.image, f.profile, r) && !r.ok);
    f.threadMasks[1] = {12, 4}; f.memory[f.positions[1] + 6] ^= 1;
    REQUIRE(!resolve(f.image, f.profile, r) && !r.ok && r.failedPattern == 1 && !r.candidates);
    REQUIRE(r.candidateCounts[0] == 1 && r.candidateCounts[2] == 1);
}
// Discovery snapshots stay immutable when another observer changes an entry before installation.
void entrySnapshots() {
    Fixture f; Resolution r;
    REQUIRE(resolve(f.image, f.profile, r));
    const auto captured = r.entryBytes;
    REQUIRE(validateResolved(f.image, f.profile, r));
    f.memory[f.positions[0] + 1] ^= 1;
    REQUIRE(r.entryBytes == captured && r.entryBytes[0][1] != f.memory[f.positions[0] + 1]);
    REQUIRE(!validateResolved(f.image, f.profile, r) && !r.ok && r.entryBytes == captured);
    f.memory[f.positions[0] + 1] ^= 1;
    REQUIRE(validateResolved(f.image, f.profile, r));
    f.memory[f.positions[0] + 5] ^= 1; // A normalized operand still must match the discovery-time entry bytes.
    REQUIRE(!validateResolved(f.image, f.profile, r) && !r.ok && !std::strcmp(r.error, "Resolved entry changed"));
    REQUIRE(r.entryBytes == captured);
}
} // namespace

// Run a compact set of structural tests; no game modules or external files are touched.
int main() {
    validRelocation(); candidateIdentity(); semanticRejections(); malformedInputs(); entrySnapshots();
    std::puts("PASS: mapped PE relocation, unique pdata starts, hash/mask, call/window and thread-import guards.");
    return 0;
}
