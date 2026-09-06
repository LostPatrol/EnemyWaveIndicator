// Fixed host-local regions, coalesced only after exact successful-spawn attribution; no engine calls.
#pragma once
#include "SpawnAttribution.h"
#include <array>
namespace nwi {
class OriginRegions {
public:
    static constexpr uint32_t Capacity = 8;
    static constexpr uint64_t LifetimeMs = 8000; // Visibility extends from the most recent spawn in this region.
    static constexpr float RadiusCm = 800.0f; // Group nearby origins, keeping the first actual point, not an average.
    struct Region { SpawnKey point{}; uint64_t wave = 0, expires = 0; int32_t serial = 0, visible = 0; };
    std::array<Region, Capacity> items{};
    uint64_t overflow = 0, stale = 0;
    uint64_t lifetimeMs = LifetimeMs; // Clamped controller preference, sampled before consuming events.
    void reset() noexcept { *this = {}; }
    void expire(uint64_t now) noexcept {
        for (auto& r : items) if (r.visible && now >= r.expires) { r.visible = 0; ++r.serial; }
    }
    bool add(const SpawnEvent& event, uint64_t now) noexcept {
        if (now < event.capturedMs || now - event.capturedMs > 250) { ++stale; return false; }
        expire(now);
        for (auto& r : items) {
            const float x = r.point.x - event.origin.x, y = r.point.y - event.origin.y, z = r.point.z - event.origin.z;
            if (r.visible && r.wave == event.wave && x*x + y*y + z*z <= RadiusCm*RadiusCm) {
                r.expires = now + lifetimeMs; ++r.serial; return true;
            }
        }
        for (auto& r : items) if (!r.visible) {
            r.point = event.origin; r.wave = event.wave; r.expires = now + lifetimeMs;
            ++r.serial; r.visible = 1; return true;
        }
        ++overflow; return false; // Explicit telemetry loss, never additional gameplay work or hidden allocation.
    }
};
} // namespace nwi
