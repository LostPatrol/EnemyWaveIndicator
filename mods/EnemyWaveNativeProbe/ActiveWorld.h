// Obtain the current World from the live game viewport; never infer activity from a loaded map asset.
#pragma once
#include <cstdint>
namespace ewi {
struct WorldResult { void* world = nullptr; bool fault = false; };
struct WorldApi {
    void* (*findViewport)(const wchar_t*) = nullptr;
    bool (*valid)(void*) = nullptr;
    WorldResult (*worldOf)(void*) = nullptr;
};
class ActiveWorld {
public:
    static constexpr uint32_t FastFindAttempts = 8; // Initial discovery follows the dispatch readiness cadence.
    static constexpr uint64_t FindRetryMs = 5000; // Missing viewports keep retrying without frequent registry scans.
    uint32_t finds = 0, reads = 0, changes = 0, faults = 0;
    bool disabled = false;
    uintptr_t lastWorld = 0; // Historical identity token; a null travel read does not make this current.
    // Bind the host's validated object accessors before the first game-thread read.
    void configure(WorldApi api) noexcept { api_ = api; }
    // Missing startup objects are retryable; only a failed World bridge permanently stops reads.
    void* read(uint64_t now) noexcept {
        if (disabled || !api_.findViewport) return nullptr;
        if (!api_.valid(viewport_)) {
            if (now < nextFindAt_) return nullptr;
            ++finds;
            viewport_ = api_.findViewport(L"GameViewportClient");
            if (!api_.valid(viewport_)) {
                if (finds >= FastFindAttempts) nextFindAt_ = now + FindRetryMs;
                return nullptr;
            }
            nextFindAt_ = 0;
        }
        ++reads;
        const auto result = api_.worldOf(viewport_);
        if (result.fault) { ++faults; disabled = true; return nullptr; }
        if (!api_.valid(result.world)) return nullptr; // Travel may temporarily have no World.
        if (lastWorld != reinterpret_cast<uintptr_t>(result.world)) {
            lastWorld = reinterpret_cast<uintptr_t>(result.world); ++changes;
        }
        return result.world;
    }
private:
    WorldApi api_{};
    void* viewport_ = nullptr; // Revalidate before each call; this object normally survives map travel.
    uint64_t nextFindAt_ = 0; // Backoff applies only to registry discovery, never cached World reads.
};
}
