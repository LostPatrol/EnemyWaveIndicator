// Native capture contract. Production hooks only observe; the opt-in prediction build may replace one audited natural-wave center.
#pragma once
#include "SpawnAttribution.h"
#include <array>
#include "../../engine/Authoring/NwiAuthoring/Source/NwiAuthoring/Public/NwiWaveTypes.h"
namespace nwi::capture {
struct Target { void* address = nullptr; std::array<unsigned char, 24> bytes{}; };
struct Binding {
    Target normal, enqueue, actor, shrink;
    uintptr_t normalReturn = 0, enqueueReturns[2]{}, actorReturn = 0, shrinkReturn = 0;
    uintptr_t imageBase = 0, imageEnd = 0;
    std::array<uintptr_t, 6> sourceChain{}; // Includes the natural scheduler, not only its shared spawning helper.
    uint32_t threadId = 0;
    Target pool, location, group, spread, spreadCallback, center;
    Target naturalCenter; uintptr_t naturalCenterReturn = 0;
    // Exact initiating controller/event class, evaluated synchronously before queuing; -1 is unrecognized.
    int32_t (*classifySource)(void* context, void* world) = nullptr;
    // Test-only gate: return true to replace the exact natural selector call with an already sampled center.
    bool (*consumeNaturalOverride)(const SpawnKey& origin, float radius, SpawnKey& output) noexcept = nullptr;
};
struct Stats {
    uint64_t waves = 0, tagged = 0, successes = 0, failures = 0, skipped = 0, sourceRejected = 0;
    uint64_t polls = 0, delivered = 0, deliveryMaxMs = 0, deliveryMaxUs = 0;
    uint64_t normalEntries = 0, normalSiteMatches = 0, noMissionWorld = 0, contextRejected = 0;
    uint64_t naturalCenterSamples = 0, naturalCenterCalls = 0, naturalCenterOverrides = 0, naturalCenterFallbacks = 0;
    std::array<uint64_t, 8> rejectedSourceFrames{}; // First failed game-only unwind, stored as image RVAs.
    uint32_t fault = 0, hookStatus = 0;
};
bool install(const Binding&) noexcept;
void setWorld(void* world) noexcept; // Begin a fresh controller/world epoch on the verified game thread.
void stop() noexcept;
void poll(uint64_t frame) noexcept;
bool pop(SpawnEvent&) noexcept;
void* spawnManager() noexcept; // Game-thread consumer uses reflected counting buckets after registration.
// Invoke the original center selector through its trampoline; available only in the opt-in hooked build.
bool sampleNaturalCenter(void* pathfinder, const SpawnKey& origin, float radius, SpawnKey& output) noexcept;
Stats stats() noexcept;
// Pure matcher is separately tested; incomplete unwinds cannot establish provenance.
bool sourceMatches(const uintptr_t* frames, size_t count, const Binding&) noexcept;
} // namespace nwi::capture
