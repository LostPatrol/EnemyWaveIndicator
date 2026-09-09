// Read-only views of the game's counting buckets; no enemy names or descriptor significance heuristics.
#pragma once
#include <cstdint>
namespace nwi {
struct EnemyBucket {
    void** data = nullptr; int32_t count = 0, capacity = 0;
    bool valid() const noexcept { return count >= 0 && capacity >= count && count <= 65536 && (!count || data); }
    bool contains(uint64_t pawn) const noexcept {
        for (int32_t i=0; i<count; ++i) if (reinterpret_cast<uint64_t>(data[i])==pawn) return true;
        return false;
    }
};
inline bool eligibleEnemy(uint64_t pawn, const EnemyBucket& regular, const EnemyBucket& small, const EnemyBucket& critters) noexcept {
    return pawn && regular.valid() && small.valid() && critters.valid()
        && regular.contains(pawn) && !small.contains(pawn) && !critters.contains(pawn);
}
// Explicit scripted swarm types may consist of small enemies; the natural filter stays strict.
inline bool eligibleScriptedEnemy(uint64_t pawn, const EnemyBucket& regular, const EnemyBucket& small, const EnemyBucket& critters) noexcept {
    return pawn && regular.valid() && small.valid() && critters.valid()
        && (regular.contains(pawn) || small.contains(pawn)) && !critters.contains(pawn);
}
}
