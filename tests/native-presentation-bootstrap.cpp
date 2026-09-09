// Fake-host tests for bounded visual bootstrap; these do not load or simulate Unreal objects.
#include "../mods/NormalWaveNativeProbe/PresentationBootstrap.h"
#include "../mods/NormalWaveNativeProbe/ActiveWorld.h"
#include <cwchar>
#include <cstdio>

namespace {
int worlds[12]{}, classToken = 0, actorToken = 0;
void* world = nullptr;
bool cached = false, loadFails = false, spawnFails = false, prepareFails = false, refreshFails = false, badArguments = false;
unsigned finds = 0, loads = 0, spawns = 0, preparations = 0, refreshCalls = 0;
const wchar_t* expectedClass() {
    return world == &worlds[10] ? L"/Game/NormalWaveIndicator/InitSpacerig.InitSpacerig_C"
        : L"/Game/NormalWaveIndicator/InitCave.InitCave_C";
}
bool equal(const nwi::WideView* v, const wchar_t* text) {
    return v && v->size == wcslen(text) && wmemcmp(v->data, text, v->size) == 0;
}
void* find(const nwi::WideView* v) {
    ++finds;
    if (equal(v, expectedClass())) return cached ? &classToken : nullptr;
    badArguments = true; return nullptr;
}
void* load(const nwi::WideView* v) {
    ++loads; if (!equal(v, expectedClass())) badArguments = true;
    cached = !loadFails; return cached ? &classToken : nullptr;
}
bool valid(void* v) { return v != nullptr; }
void* currentWorld() { return world; }
nwi::WorldKind worldKind(void* value) {
    return value == &worlds[11] ? nwi::WorldKind::Excluded
        : value == &worlds[10] ? nwi::WorldKind::SpaceRig : nwi::WorldKind::Mission;
}
// Cold-start regression: binding cannot succeed until the entry class and its dependencies are loaded.
bool prepare(void* cls) {
    ++preparations;
    if (!cached || cls != &classToken) badArguments = true;
    return cached && !prepareFails;
}
int viewportToken = 0;
bool viewportMissing = false, bridgeFault = false;
unsigned viewportSearches = 0;
void* findViewport(const wchar_t* name) {
    ++viewportSearches;
    if (wcscmp(name, L"GameViewportClient")) badArguments = true;
    return viewportMissing ? nullptr : &viewportToken;
}
nwi::WorldResult worldOf(void* viewport) {
    if (viewport != &viewportToken) badArguments = true;
    return {world, bridgeFault};
}
void* spawn(void* w, void* cls, const nwi::Position3* p) {
    ++spawns;
    if (w != world || cls != &classToken || !preparations || !p || p->x || p->y || p->z) badArguments = true;
    return spawnFails ? nullptr : &actorToken;
}
bool refresh(void* w) { ++refreshCalls; if (w != world) badArguments = true; return !refreshFails; }
void setup(nwi::PresentationBootstrap& p) {
    world = &worlds[0]; cached = loadFails = spawnFails = prepareFails = refreshFails = badArguments = false;
    finds = loads = spawns = preparations = refreshCalls = 0;
    p.configure({find, load, valid, spawn, currentWorld, worldKind, prepare, refresh}, 0);
}
#define REQUIRE(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
}
int main() {
    {
        nwi::PresentationBootstrap p; setup(p); world = &worlds[10];
        p.tick(30000); p.tick(35000);
        REQUIRE(p.kind == nwi::WorldKind::SpaceRig && loads == 1 && preparations == 1 && spawns == 1 && refreshCalls == 1 && !badArguments);
        // A preloaded class from an integration entry still receives the native binding on mission travel.
        world = &worlds[0]; p.tick(40000); p.tick(45000);
        REQUIRE(p.kind == nwi::WorldKind::Mission && loads == 1 && preparations == 2 && spawns == 2 && !badArguments);
    }
    {
        // Mod Hub may not exist on the first post-spawn callback; retry without duplicating Init.
        nwi::PresentationBootstrap p; setup(p); refreshFails = true;
        p.tick(30000); p.tick(35000);
        REQUIRE(p.status == 7 && spawns == 1 && refreshCalls == 1);
        p.tick(35999); REQUIRE(refreshCalls == 1);
        refreshFails = false; p.tick(36000);
        REQUIRE(p.status == 3 && p.refreshes == 1 && p.refreshFailures == 1 && refreshCalls == 2 && spawns == 1);
    }
    {
        // A permanently absent Mod Hub has a strict retry cap and never becomes an actor-spawn loop.
        nwi::PresentationBootstrap p; setup(p); refreshFails = true;
        p.tick(30000); p.tick(35000);
        for (unsigned i = 1; i < nwi::PresentationBootstrap::MaxRefreshAttempts; ++i) p.tick(35000 + i * 1000);
        REQUIRE(p.status == 8 && refreshCalls == nwi::PresentationBootstrap::MaxRefreshAttempts && spawns == 1);
        for (int i = 0; i < 100; ++i) p.tick(50000 + i * 1000);
        REQUIRE(refreshCalls == nwi::PresentationBootstrap::MaxRefreshAttempts && spawns == 1);
    }
    {
        REQUIRE(nwi::classifyWorldName(L"", 0) == nwi::WorldKind::Excluded);
        const wchar_t* names[] = {
            L"World /Game/Maps/LVL_StartingScreen.LVL_StartingScreen",
            L"World /Game/Maps/UILevels/LVL_Loading_StartMission01.LVL_Loading_StartMission01",
            L"World /Game/Maps/UILevels/LVL_CharacterSelection.LVL_CharacterSelection",
            L"World /Game/Maps/SpaceRig/LVL_SpaceRig.LVL_SpaceRig",
            L"World /Game/Maps/LVL_Procedural.LVL_Procedural"
        };
        for (int i = 0; i < 5; ++i) {
            const auto expected = i < 3 ? nwi::WorldKind::Excluded : i == 3 ? nwi::WorldKind::SpaceRig : nwi::WorldKind::Mission;
            REQUIRE(nwi::classifyWorldName(names[i], wcslen(names[i])) == expected);
            REQUIRE(nwi::classifyWorldName(names[i], wcslen(names[i]) - 1) == nwi::WorldKind::Excluded);
        }
        nwi::PresentationBootstrap p; setup(p); world = &worlds[11];
        for (int i = 0; i < 100; ++i) p.tick(30000 + i * 1000);
        REQUIRE(p.status == 6 && !spawns && !loads && p.excluded == 1 && !p.attempted);
        world = &worlds[0]; p.tick(140000); p.tick(145000);
        REQUIRE(spawns == 1 && p.status == 3);
        world = &worlds[11]; p.tick(150000);
        world = &worlds[0]; p.tick(160000);
        REQUIRE(spawns == 1 && p.status == 3 && p.excluded == 2);
    }
    {
        // A persistent viewport changes World during travel; cached registry lookup is not per-frame scanning.
        nwi::ActiveWorld reader; viewportSearches = 0; viewportMissing = bridgeFault = false;
        reader.configure({findViewport, valid, worldOf});
        world = &worlds[0]; REQUIRE(reader.read() == world);
        for (int i = 0; i < 100; ++i) REQUIRE(reader.read() == world);
        world = nullptr; REQUIRE(reader.read() == nullptr && !reader.disabled);
        world = &worlds[1]; REQUIRE(reader.read() == world);
        REQUIRE(reader.changes == 2 && viewportSearches == 1);
        bridgeFault = true; REQUIRE(reader.read() == nullptr && reader.faults == 1 && reader.disabled);
        const auto reads = reader.reads; bridgeFault = false;
        REQUIRE(reader.read() == nullptr && reader.reads == reads);
    }
    {
        nwi::ActiveWorld reader; viewportMissing = true; viewportSearches = 0;
        reader.configure({findViewport, valid, worldOf});
        for (int i = 0; i < 100; ++i) REQUIRE(reader.read() == nullptr);
        REQUIRE(viewportSearches == 8 && reader.disabled); viewportMissing = false;
    }
    {
        nwi::PresentationBootstrap p; setup(p);
        p.tick(29999); REQUIRE(finds == 0);
        p.tick(30000); p.tick(34999); REQUIRE(spawns == 0);
        p.tick(35000); REQUIRE(spawns == 1 && loads == 1 && p.status == 3);
        for (int i = 0; i < 100; ++i) p.tick(36000 + i);
        REQUIRE(spawns == 1 && !badArguments);
        // Class pointers are reacquired, not retained across a world change/GC.
        world = &worlds[1]; cached = false; p.tick(40000); p.tick(45000);
        REQUIRE(spawns == 2 && loads == 2 && p.spawned == 2);
        for (int i = 2; i < 12; ++i) {
            world = &worlds[i]; p.tick(50000 + i * 10000); p.tick(55000 + i * 10000);
        }
        REQUIRE(p.status == 5 && p.attempted == 8 && spawns == 8 && !badArguments);
    }
    for (int failure = 0; failure < 3; ++failure) {
        nwi::PresentationBootstrap p; setup(p); loadFails = failure == 0; spawnFails = failure == 1; prepareFails = failure == 2;
        p.tick(30000); p.tick(35000); REQUIRE(p.status == 4);
        for (int i = 0; i < 100; ++i) p.tick(40000 + i * 5000);
        REQUIRE(loads == 1 && spawns == static_cast<unsigned>(failure == 1) && !badArguments);
    }
    {
        nwi::PresentationBootstrap p; setup(p); p.tick(30000);
        world = nullptr; p.tick(34000); world = &worlds[0]; p.tick(35000);
        p.tick(39999); REQUIRE(spawns == 0); p.tick(40000); REQUIRE(spawns == 1);
    }
    {
        nwi::PresentationBootstrap p; setup(p); p.tick(30000); p.tick(35000);
        world = &worlds[1]; p.tick(40000);
        world = &worlds[0]; p.tick(44000); // Old active world returns; discard the interrupted candidate.
        world = &worlds[1]; p.tick(45000); p.tick(49999); REQUIRE(spawns == 1);
        p.tick(50000); REQUIRE(spawns == 2 && !badArguments);
        // find() accepts only our class name: loaded map paths can never influence bootstrap.
    }
    puts("PASS: cached viewport, bounded Mod Hub refresh, null travel world, bridge fault stop, eight lookup cap, active-world-only bootstrap, interrupted travel, reuse and creation limits.");
}
