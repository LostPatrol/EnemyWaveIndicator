// Engine-independent geometry and one-shot countdown gate for the opt-in natural-wave prediction test.
#pragma once
#include <cmath>
#include <cstdint>

namespace nwi::prediction {
inline constexpr uint32_t RegionType = 255; // Reserved test-only handoff value; stock wave IDs occupy 0..46.
inline constexpr float LeadSeconds = 5.0f;
inline constexpr float ProjectionPaddingCm = 300.0f; // Matches the game's current natural-wave projection padding.
inline constexpr float SpawnDistanceCm = 3000.0f; // Matches the game's current natural-wave search extension.

struct Position { float x = 0, y = 0, z = 0; };
struct Geometry { Position center{}; float radius = 0; bool valid = false; };

inline bool finite(const Position& point) noexcept {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
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
