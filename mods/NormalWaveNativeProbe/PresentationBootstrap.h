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
    bool (*prepareClass)(void*) = nullptr;
    bool (*refreshModHub)(void*) = nullptr;
};
class PresentationBootstrap {
public:
    static constexpr uint32_t MaxWorlds = 8; // Bound all object creation, including a failed attempt.
    static constexpr uint32_t MaxRefreshAttempts = 8; // Mod Hub may finish BeginPlay shortly after our Init actor.
    uint32_t status = 0, spawned = 0, loads = 0, attempted = 0;
    uint32_t refreshAttempts = 0, refreshes = 0, refreshFailures = 0;
    uint32_t excluded = 0;
    WorldKind kind = WorldKind::Excluded;
    void configure(PresentationApi api, uint64_t now) noexcept { api_ = api; readyAt_ = now + 30000; status = 1; }
    void tick(uint64_t now) noexcept {
        if (!api_.find || !api_.activeWorld || !api_.worldKind || status == 4 || status == 5 || status == 8 || now < readyAt_) return;
        if (attempted >= MaxWorlds) { status = 5; return; }
        {
            void* world = api_.activeWorld();
            if (!api_.valid(world)) { candidate_ = checkedWorld_ = nullptr; return; }
            if (checkedWorld_ != world) {
                checkedWorld_ = world; kind = api_.worldKind(world);
                if (kind == WorldKind::Excluded) ++excluded;
            }
            if (kind == WorldKind::Excluded) { candidate_ = nullptr; status = 6; return; }
            if (pendingRefreshWorld_ && pendingRefreshWorld_ != world) clearRefresh();
            if (pendingRefreshWorld_ == world) {
                if (now >= nextRefreshAt_) tryRefresh(world, now);
                return;
            }
            bool seen = false;
            for (uint32_t i = 0; i < attempted; ++i) if (worlds_[i] == world) seen = true;
            if (seen) { candidate_ = nullptr; status = 3; return; }
            if (candidate_ != world) { candidate_ = world; candidateSince_ = now; status = 2; return; }
            if (now - candidateSince_ < 5000) return;
            // Loading Init also loads its hard-referenced controller before binding NwiPoll.
            // Init owns the authority check and GetActorOfClass deduplication with MintCat's entry.
            const auto name = kind == WorldKind::SpaceRig ? view(RigClassPath) : view(CaveClassPath);
            void* cls = api_.find(&name); // Never keep an unrooted UClass pointer across callbacks/GC.
            if (!api_.valid(cls)) { ++loads; cls = api_.loadClass(&name); }
            if (!api_.valid(cls)) { status = 4; return; }
            if (api_.prepareClass && !api_.prepareClass(cls)) { status = 4; return; }
            worlds_[attempted++] = world; // Commit before spawn: failures cannot turn into a spawn loop.
            const Position3 zero{0, 0, 0};
            if (!api_.valid(api_.spawn(world, cls, &zero))) { status = 4; return; }
            ++spawned;
            if (api_.refreshModHub) {
                pendingRefreshWorld_ = world; refreshAttemptsForWorld_ = 0;
                tryRefresh(world, now);
            } else status = 3;
            return;
        }
    }
    inline static constexpr wchar_t RigClassPath[] = L"/Game/NormalWaveIndicator/InitSpacerig.InitSpacerig_C";
    inline static constexpr wchar_t CaveClassPath[] = L"/Game/NormalWaveIndicator/InitCave.InitCave_C";
private:
    template<size_t N> static WideView view(const wchar_t (&text)[N]) noexcept { return {text, N - 1}; }
    PresentationApi api_{};
    void clearRefresh() noexcept { pendingRefreshWorld_ = nullptr; refreshAttemptsForWorld_ = 0; nextRefreshAt_ = 0; }
    void tryRefresh(void* world, uint64_t now) noexcept {
        ++refreshAttempts; ++refreshAttemptsForWorld_;
        if (api_.refreshModHub(world)) { ++refreshes; clearRefresh(); status = 3; return; }
        ++refreshFailures;
        if (refreshAttemptsForWorld_ >= MaxRefreshAttempts) { clearRefresh(); status = 8; return; }
        nextRefreshAt_ = now + 1000; status = 7;
    }
    void* worlds_[MaxWorlds]{}; // Identity tokens only; old world pointers are never dereferenced.
    void* pendingRefreshWorld_ = nullptr;
    uint32_t refreshAttemptsForWorld_ = 0;
    void* candidate_ = nullptr;
    void* checkedWorld_ = nullptr; // Cache name classification only while this active World is unchanged.
    uint64_t candidateSince_ = 0, readyAt_ = 0, nextRefreshAt_ = 0;
};
} // namespace nwi
