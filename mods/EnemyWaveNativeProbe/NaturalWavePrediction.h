// Engine-independent geometry and one-shot countdown gate for the opt-in natural-wave prediction test.
#pragma once
#include <cmath>
#include <cstdint>

namespace nwi::prediction {
inline constexpr uint32_t RegionType = 255; // Reserved test-only handoff value; stock wave IDs occupy 0..46.
inline constexpr float LeadSeconds = 5.0f;
inline constexpr float ProjectionPaddingCm = 300.0f; // Matches the game's current natural-wave projection padding.
inline constexpr float SpawnDistanceCm = 3000.0f; // Matches the game's current natural-wave search extension.
inline constexpr float ShellDepthCm = 1000.0f; // Prefer the outer 10 m of the stock search radius.
inline constexpr float MovementToleranceCm = 5.0f; // Allow tiny idle/root jitter while rejecting meaningful movement.
inline constexpr float InputToleranceCm = 25.0f; // Projected selector input may vary slightly between adjacent frames.
inline constexpr uint64_t MaximumLockAgeMs = 7500; // Refuse stale samples from a delayed or cancelled countdown.
inline constexpr uintptr_t WorldNavigationOwnerOffset = 0x120; // Current selector's UWorld intermediate object.
inline constexpr uintptr_t NavigationOffset = 0x420; // Navigation wrapper on the intermediate owner.
inline constexpr uintptr_t PathfinderOffset = 0x708; // Query object used by both audited navigation helpers.
inline constexpr uintptr_t PlayerControllerPawnOffset = 0x250; // Pawn member read by the stock selector.
inline constexpr uintptr_t ActorRootOffset = 0x130; // RootComponent member in the current game image.
inline constexpr uintptr_t SceneTranslationOffset = 0x1d0; // Component world translation used by the selector.

struct Position { float x = 0, y = 0, z = 0; };
struct Geometry { Position center{}; float radius = 0; bool valid = false; };

inline float distanceSquared(const Position& a, const Position& b) noexcept {
    const float x = a.x-b.x, y = a.y-b.y, z = a.z-b.z;
    return x*x + y*y + z*z;
}

inline bool finite(const Position& point) noexcept {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

inline bool positionsNear(const Position& a, const Position& b, float tolerance) noexcept {
    return finite(a) && finite(b) && std::isfinite(tolerance) && tolerance >= 0
        && distanceSquared(a, b) <= tolerance * tolerance;
}

// Reproduce only the selector's pointer lookup; callers separately check Pathfinder readiness.
inline void* resolvePathfinder(void* world) noexcept {
    if (!world) return nullptr;
    auto* owner = *reinterpret_cast<void**>(static_cast<unsigned char*>(world) + WorldNavigationOwnerOffset);
    auto* navigation = owner
        ? *reinterpret_cast<void**>(static_cast<unsigned char*>(owner) + NavigationOffset) : nullptr;
    return navigation
        ? *reinterpret_cast<void**>(static_cast<unsigned char*>(navigation) + PathfinderOffset) : nullptr;
}

// Read the same actor-root translation used by the selector, without invoking an actor method.
inline bool readActorPosition(void* actor, Position& result) noexcept {
    if (!actor) return false;
    auto* root = *reinterpret_cast<void**>(static_cast<unsigned char*>(actor) + ActorRootOffset);
    if (!root) return false;
    result = *reinterpret_cast<Position*>(static_cast<unsigned char*>(root) + SceneTranslationOffset);
    return finite(result);
}

// Deterministic farthest-first representatives cover the navigable outer shell without consuming game RNG.
inline uint32_t selectShellCandidates(const Position* points, uint32_t count, const Position& center,
    float radius, Position* output, uint32_t capacity) noexcept {
    if (!points || !count || !finite(center) || !std::isfinite(radius) || radius <= 0 || !output || !capacity) return 0;
    const float innerRadius = radius > ShellDepthCm ? radius - ShellDepthCm : 0.0f;
    const float innerSquared = innerRadius * innerRadius;
    bool shellAvailable = false;
    for (uint32_t i = 0; i < count; ++i)
        if (finite(points[i]) && distanceSquared(points[i], center) >= innerSquared) { shellAvailable = true; break; }
    uint32_t selected = 0;
    while (selected < capacity) {
        uint32_t best = count; float bestScore = -1.0f;
        for (uint32_t i = 0; i < count; ++i) {
            if (!finite(points[i])) continue;
            const float radial = distanceSquared(points[i], center);
            if (shellAvailable && radial < innerSquared) continue;
            float score = radial;
            if (selected) {
                score = distanceSquared(points[i], output[0]);
                for (uint32_t j = 1; j < selected; ++j) {
                    const float candidate = distanceSquared(points[i], output[j]);
                    if (candidate < score) score = candidate;
                }
            }
            if (score > bestScore) { bestScore = score; best = i; }
        }
        // Stop when only duplicate/sub-meter grid points remain.
        if (best == count || (selected && bestScore < 10000.0f)) break;
        output[selected++] = points[best];
    }
    return selected;
}

// Reproduce the deterministic player-sphere portion of the game's natural-wave selector.
inline Geometry playerSphere(const Position* players, uint32_t count) noexcept {
    Geometry result;
    if (!players || !count || count > 4) return result;
    for (uint32_t i = 0; i < count; ++i) if (!finite(players[i])) return result;
    uint32_t first = 0, second = 0;
    float farthestSquared = -1.0f;
    for (uint32_t i = 0; i < count; ++i) for (uint32_t j = i; j < count; ++j) {
        const float x = players[i].x - players[j].x;
        const float y = players[i].y - players[j].y;
        const float z = players[i].z - players[j].z;
        const float squared = x*x + y*y + z*z;
        if (squared > farthestSquared) { farthestSquared = squared; first = i; second = j; }
    }
    result.center = {(players[first].x + players[second].x) * 0.5f,
        (players[first].y + players[second].y) * 0.5f,
        (players[first].z + players[second].z) * 0.5f};
    result.radius = std::sqrt(farthestSquared) * 0.5f;
    result.valid = finite(result.center) && std::isfinite(result.radius);
    return result;
}

// Fire once when a countdown first enters the prediction window; re-arm only after a new long interval appears.
class CountdownGate {
public:
    bool sample(float seconds, bool enabled, bool blocked) noexcept {
        if (!std::isfinite(seconds)) { armed_ = false; return false; }
        if (seconds > LeadSeconds + 0.25f) armed_ = true;
        if (!enabled || blocked || !armed_ || seconds <= 0 || seconds > LeadSeconds) return false;
        armed_ = false;
        return true;
    }
    void reset() noexcept { armed_ = true; }
private:
    bool armed_ = true;
};
} // namespace nwi::prediction
