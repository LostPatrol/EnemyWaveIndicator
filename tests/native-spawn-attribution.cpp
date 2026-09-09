// Deterministic semantic tests against an independent vector model; no Unreal DLL or gameplay hook is used.
#include "../mods/EnemyWaveNativeProbe/SpawnAttribution.h"
#include <cstdio>
#include <limits>
#include <vector>

#define REQUIRE(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main() {
    using namespace nwi;
    const SpawnKey a{10, 1, 2, 3}, b{20, 4, 5, 6};
    SpawnAttribution ledger; SpawnEvent event;
    REQUIRE(ledger.begin(1, 99, 0));
    REQUIRE(ledger.append(1, 99, 0, 1, a, 100));
    REQUIRE(ledger.append(1, 99, 1, 2, a, 0)); // Identical coordinates/descriptor, different provenance.
    REQUIRE(ledger.append(1, 99, 2, 3, b, 101));
    REQUIRE(ledger.removeSwap(1, 99, 0, 3, 2, a, 1, 10)); // b moves into slot 0.
    REQUIRE(ledger.removeSwap(1, 99, 1, 2, 1, a, 2, 10)); // Non-normal duplicate must never emit.
    REQUIRE(ledger.removeSwap(1, 99, 0, 1, 0, b, 0, 10)); // Failed creation consumes b's tag.
    REQUIRE(ledger.pop(1, 10, 0, event) && event.wave == 100 && event.pawn == 1);
    REQUIRE(!ledger.pop(1, 10, 0, event));
    REQUIRE(ledger.stats.failed == 1 && ledger.stats.unclassified == 1);
    REQUIRE(ledger.append(1, 99, 0, 1, b, 0));
    REQUIRE(ledger.removeSwap(1, 99, 0, 1, 0, b, 3, 11));
    REQUIRE(!ledger.pop(1, 11, 0, event)); // No stale provenance from the failed spawn.

    // A missed queue mutation or changed key disables the epoch and discards already queued output.
    REQUIRE(ledger.append(1, 99, 0, 1, a, 102));
    REQUIRE(!ledger.removeSwap(1, 99, 0, 1, 0, b, 4, 12));
    REQUIRE(ledger.fault() == AttributionFault::Key && ledger.pending() == 0);
    REQUIRE(!ledger.append(1, 99, 0, 1, a, 102));
    REQUIRE(!ledger.begin(1, 99, 0)); // Same world epoch cannot silently restart after a fault.
    REQUIRE(ledger.begin(2, 99, 0)); // Reused manager address with a new mission epoch is safe.
    REQUIRE(!ledger.append(2, 99, 1, 2, a, 103) && ledger.fault() == AttributionFault::Count);
    REQUIRE(!ledger.begin(3, 99, 1)); // Mid-queue attachment cannot label preexisting entries.
    REQUIRE(ledger.begin(4, 99, 0));
    REQUIRE(!ledger.append(4, 100, 0, 1, a, 1) && ledger.fault() == AttributionFault::Context);
    REQUIRE(ledger.begin(5, 99, 0));
    auto bad = a; bad.x = std::numeric_limits<float>::quiet_NaN();
    REQUIRE(!ledger.append(5, 99, 0, 1, bad, 1));

    // Fixed bounds limit telemetry only; overflow never requests a retry or changes game queue data.
    REQUIRE(ledger.begin(6, 99, 0));
    for (uint32_t i = 0; i < SpawnAttribution::Capacity; ++i) REQUIRE(ledger.append(6, 99, i, i + 1, a, 1));
    REQUIRE(!ledger.append(6, 99, 512, 513, a, 1) && ledger.fault() == AttributionFault::Capacity);
    REQUIRE(ledger.begin(7, 99, 0));
    for (uint32_t i = 0; i < 600; ++i) {
        REQUIRE(ledger.append(7, 99, 0, 1, a, 1));
        REQUIRE(ledger.removeSwap(7, 99, 0, 1, 0, a, i + 1, 100));
    }
    REQUIRE(ledger.pending() == 512 && ledger.stats.dropped == 88 && ledger.tracked() == 0);
    REQUIRE(!ledger.pop(6, 100, 0, event) && ledger.pending() == 512); // Wrong consumer context.
    REQUIRE(!ledger.pop(7, 102, 1, event) && ledger.stats.dropped == 600); // No delayed replay.
    REQUIRE(ledger.append(7, 99, 0, 1, a, 1));
    REQUIRE(ledger.removeSwap(7, 99, 0, 1, 0, a, 1, 100));
    REQUIRE(ledger.begin(8, 99, 0) && !ledger.pop(8, 100, 0, event)); // Travel discards old output.

    // Reference model exercises mixed waves, duplicate keys, arbitrary swap removal and backing reallocation.
    struct Reference { SpawnKey key; uint64_t wave; };
    std::vector<Reference> reference;
    uint32_t random = 1729;
    uint64_t expectedPublished = 0;
    for (uint64_t step = 0; step < 100000; ++step) {
        random = random * 1664525u + 1013904223u;
        const auto n = static_cast<uint32_t>(reference.size());
        if (!n || (n < 400 && (random & 15u) < 9u)) {
            const SpawnKey key{1 + (random % 11u), static_cast<float>(random % 3u), 0, 0};
            const uint64_t wave = random % 4u ? 1 + random % 7u : 0;
            reference.push_back({key, wave});
            REQUIRE(ledger.append(8, 99, n, n + 1, key, wave));
        } else {
            const uint32_t index = random % n;
            const auto item = reference[index];
            const uint64_t pawn = random % 5u ? step + 1 : 0;
            reference[index] = reference.back(); reference.pop_back();
            REQUIRE(ledger.removeSwap(8, 99, index, n, n - 1, item.key, pawn, step));
            const bool expected = pawn && item.wave;
            REQUIRE(ledger.pop(8, step, 0, event) == expected);
            if (expected) {
                ++expectedPublished;
                REQUIRE(event.wave == item.wave && event.pawn == pawn && event.origin == item.key);
            }
            REQUIRE(!ledger.pop(8, step, 0, event));
        }
        REQUIRE(ledger.tracked() == reference.size());
    }
    REQUIRE(ledger.stats.published == expectedPublished && expectedPublished > 1000);
    std::printf("PASS: mixed/duplicate origins, swap removal, failure, stale epochs, overflow, 100000 reference operations; storage=%zu bytes.\n", sizeof(ledger));
}
