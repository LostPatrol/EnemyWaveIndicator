// Native observer contract. Hooks forward unchanged arguments exactly once; presentation runs elsewhere.
#pragma once
#include "SpawnAttribution.h"
#include <array>
namespace nwi::capture {
struct Target { void* address = nullptr; std::array<unsigned char, 24> bytes{}; };
struct Binding {
    Target normal, enqueue, actor, shrink;
    uintptr_t normalReturn = 0, enqueueReturns[2]{}, actorReturn = 0, shrinkReturn = 0;
    uintptr_t imageBase = 0, imageEnd = 0;
    std::array<uintptr_t, 5> sourceChain{}; // Exact game return addresses; first slot allows enqueueReturns[1].
    uint32_t threadId = 0;
};
struct Stats {
    uint64_t waves = 0, tagged = 0, successes = 0, failures = 0, skipped = 0, sourceRejected = 0;
    uint64_t polls = 0, delivered = 0, deliveryMaxMs = 0, deliveryMaxUs = 0;
    uint64_t normalEntries = 0, normalSiteMatches = 0, noMissionWorld = 0, contextRejected = 0;
    std::array<uint64_t, 8> rejectedSourceFrames{}; // First failed game-only unwind, stored as image RVAs.
    uint32_t fault = 0, hookStatus = 0;
};
bool install(const Binding&) noexcept;
void setWorld(void* world) noexcept; // Called by our host-only renderer, on the verified game thread.
void stop() noexcept;
void poll(uint64_t frame) noexcept;
bool pop(SpawnEvent&) noexcept;
Stats stats() noexcept;
// Pure matcher is separately tested; incomplete unwinds cannot establish provenance.
bool sourceMatches(const uintptr_t* frames, size_t count, const Binding&) noexcept;
} // namespace nwi::capture
