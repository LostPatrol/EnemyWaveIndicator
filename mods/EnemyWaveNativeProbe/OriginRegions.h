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
    struct Region {
        SpawnKey point{}; uint64_t wave = 0, expires = 0; int32_t serial = 0, visible = 0;
        float weight = 0; uint32_t count = 0;
        // Sphere volume scales with admitted successful enemy cost (200 = twenty base grunts).
        float scale() const noexcept { return std::fmin(4.f, std::fmax(.4f, std::cbrt(weight / 200.f))); }
    };
    std::array<Region, Capacity> items{};
    uint64_t overflow = 0, stale = 0;
    uint64_t lifetimeMs = LifetimeMs; // Clamped controller preference, sampled before consuming events.
    void reset() noexcept { *this = {}; }
    void expire(uint64_t now) noexcept {
        for (auto& r : items) if (r.visible && now >= r.expires) { r.visible = 0; ++r.serial; }
    }
    bool add(const SpawnEvent& event, uint64_t now, float cost = 10.f) noexcept {
        if (!std::isfinite(cost) || cost < 0) return false;
        if (now < event.capturedMs || now - event.capturedMs > 250) { ++stale; return false; }
        expire(now);
        const auto& point = event.center.valid() ? event.center : event.origin;
        for (auto& r : items) {
            const float x = r.point.x - point.x, y = r.point.y - point.y, z = r.point.z - point.z;
            const bool sameCenter = event.center.valid() ? x == 0 && y == 0 && z == 0 : x*x + y*y + z*z <= RadiusCm*RadiusCm;
            if (r.visible && r.wave == event.wave && sameCenter) {
                r.expires = now + lifetimeMs; r.weight += cost; ++r.count; ++r.serial; return true;
            }
        }
        for (auto& r : items) if (!r.visible) {
            r.point = point; r.wave = event.wave; r.expires = now + lifetimeMs; r.weight = cost; r.count = 1;
            ++r.serial; r.visible = 1; return true;
        }
        ++overflow; return false; // Explicit telemetry loss, never additional gameplay work or hidden allocation.
    }
};
} // namespace nwi
