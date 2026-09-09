// Load and start our deduplicating Init actor on the verified game thread, including loose-Pak installs.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <iterator>

namespace nwi {
enum class WorldKind : uint32_t { Excluded, SpaceRig, Mission };
// Classify the active object's complete name, not another loaded map found by a global path search.
inline WorldKind classifyWorldName(const wchar_t* name, size_t length) noexcept {
    constexpr wchar_t rig[] = L"World /Game/Maps/SpaceRig/LVL_SpaceRig.LVL_SpaceRig";
    constexpr wchar_t mission[] = L"World /Game/Maps/LVL_Procedural.LVL_Procedural";
    if (length == std::size(rig) - 1 && wmemcmp(name, rig, length) == 0) return WorldKind::SpaceRig;
    if (length == std::size(mission) - 1 && wmemcmp(name, mission, length) == 0) return WorldKind::Mission;
    return WorldKind::Excluded;
}
struct WideView { const wchar_t* data; size_t size; }; // Borrowed UTF-16 view, never transferred ownership.
struct Position3 { double x, y, z; }; // Host converts these doubles to UE4 FVector floats.
static_assert(sizeof(WideView) == 16 && sizeof(Position3) == 24);
struct PresentationApi {
    void* (*find)(const WideView*) = nullptr;
    void* (*loadClass)(const WideView*) = nullptr;
    bool (*valid)(void*) = nullptr;
    void* (*spawn)(void*, void*, const Position3*) = nullptr;
    void* (*activeWorld)() = nullptr;
    WorldKind (*worldKind)(void*) = nullptr;
    bool (*worldReady)(void*) = nullptr;
    bool (*controllerReady)(void*) = nullptr;
    bool (*prepareClass)(void*) = nullptr;
    bool (*refreshModHub)(void*) = nullptr;
};
class PresentationBootstrap {
public:
    static constexpr uint32_t MaxPreparationAttempts = 8; // Bound retries for missing/temporarily unavailable owned assets.
    static constexpr uint32_t MaxRecoveryAttempts = 8; // Bound recovery if the owned controller repeatedly disappears in one World.
    static constexpr uint32_t MaxRefreshAttempts = 8; // Bound Mod Hub calls even if its live actor changes during travel.
    uint32_t status = 0, spawned = 0, loads = 0, attempted = 0;
    uint32_t readinessChecks = 0, preparationFailures = 0;
    uint32_t refreshAttempts = 0, refreshes = 0, refreshFailures = 0;
    uint32_t excluded = 0;
    WorldKind kind = WorldKind::Excluded;
    void configure(PresentationApi api) noexcept { api_ = api; status = 1; }
    void tick() noexcept {
        if (!api_.find || !api_.activeWorld || !api_.worldKind || !api_.worldReady) return;
        {
            void* world = api_.activeWorld();
            if (!api_.valid(world)) { leaveWorld(); return; }
            if (checkedWorld_ != world) beginWorld(world);
            if (kind == WorldKind::Excluded) { status = 6; return; }

            // UWorld addresses can be reused and a slow poll can miss the intervening loading map.
            // Verify our owned controller instead of treating a historical pointer as a lifetime ID.
            if (spawnSucceededForWorld_) {
                if (api_.controllerReady && !api_.controllerReady(world)) {
                    spawnAttemptedForWorld_ = spawnSucceededForWorld_ = false;
                    preparationAttemptsForWorld_ = 0;
                    clearRefresh();
                    status = 1;
                } else {
                    if (status == 8) return;
                    if (pendingRefreshWorld_ == world) { tryRefresh(world); return; }
                    status = 3;
                    return;
                }
            }
            if (status == 4 || status == 8 || spawnAttemptedForWorld_) return;
            if (recoveryAttemptsForWorld_ >= MaxRecoveryAttempts) { status = 4; return; }
            ++readinessChecks;
            if (!api_.worldReady(world)) { status = 2; return; }
            // Loading Init also loads its hard-referenced controller before binding NwiPoll.
            // Init owns the authority check and GetActorOfClass deduplication with MintCat's entry.
            const auto name = kind == WorldKind::SpaceRig ? view(RigClassPath) : view(CaveClassPath);
            void* cls = api_.find(&name); // Never keep an unrooted UClass pointer across callbacks/GC.
            if (!api_.valid(cls)) { ++loads; cls = api_.loadClass(&name); }
            if (!api_.valid(cls)) { failPreparation(); return; }
            if (api_.prepareClass && !api_.prepareClass(cls)) { failPreparation(); return; }
            ++attempted; ++recoveryAttemptsForWorld_;
            spawnAttemptedForWorld_ = true; // Commit before spawn: failures cannot turn into a spawn loop.
            const Position3 zero{0, 0, 0};
            if (!api_.valid(api_.spawn(world, cls, &zero))) { status = 4; return; }
            spawnSucceededForWorld_ = true; ++spawned;
            if (api_.refreshModHub) {
                pendingRefreshWorld_ = world; refreshAttemptsForWorld_ = 0;
                tryRefresh(world);
            } else status = 3;
            return;
        }
    }
    inline static constexpr wchar_t RigClassPath[] = L"/Game/EnemyWaveIndicator/InitSpacerig.InitSpacerig_C";
    inline static constexpr wchar_t CaveClassPath[] = L"/Game/EnemyWaveIndicator/InitCave.InitCave_C";
private:
    template<size_t N> static WideView view(const wchar_t (&text)[N]) noexcept { return {text, N - 1}; }
    PresentationApi api_{};
    void beginWorld(void* world) noexcept {
        checkedWorld_ = world;
        kind = api_.worldKind(world);
        if (kind == WorldKind::Excluded) ++excluded;
        preparationAttemptsForWorld_ = recoveryAttemptsForWorld_ = 0;
        spawnAttemptedForWorld_ = spawnSucceededForWorld_ = false;
        clearRefresh();
        status = kind == WorldKind::Excluded ? 6 : 1;
    }
    void leaveWorld() noexcept {
        checkedWorld_ = nullptr;
        kind = WorldKind::Excluded;
        preparationAttemptsForWorld_ = recoveryAttemptsForWorld_ = 0;
        spawnAttemptedForWorld_ = spawnSucceededForWorld_ = false;
        clearRefresh();
        status = 1; // Keep the fast readiness cadence active through map travel.
    }
    void failPreparation() noexcept {
        ++preparationFailures; ++preparationAttemptsForWorld_;
        status = preparationAttemptsForWorld_ >= MaxPreparationAttempts ? 4 : 2;
    }
    void clearRefresh() noexcept { pendingRefreshWorld_ = nullptr; refreshAttemptsForWorld_ = 0; }
    void tryRefresh(void* world) noexcept {
        ++refreshAttempts; ++refreshAttemptsForWorld_;
        if (api_.refreshModHub(world)) { ++refreshes; clearRefresh(); status = 3; return; }
        ++refreshFailures;
        if (refreshAttemptsForWorld_ >= MaxRefreshAttempts) { clearRefresh(); status = 8; return; }
        status = 7;
    }
    void* pendingRefreshWorld_ = nullptr;
    uint32_t refreshAttemptsForWorld_ = 0;
    uint32_t preparationAttemptsForWorld_ = 0;
    uint32_t recoveryAttemptsForWorld_ = 0;
    void* checkedWorld_ = nullptr; // Cache name classification only while this active World is unchanged.
    bool spawnAttemptedForWorld_ = false;
    bool spawnSucceededForWorld_ = false;
};
} // namespace nwi
