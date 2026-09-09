// Fake-host tests for bounded visual bootstrap; these do not load or simulate Unreal objects.
#include "../mods/NormalWaveNativeProbe/PresentationBootstrap.h"
#include "../mods/NormalWaveNativeProbe/ModHubRegistration.h"
#include "../mods/NormalWaveNativeProbe/ActiveWorld.h"
#include <cwchar>
#include <cstdio>

namespace {
int worlds[12]{}, classToken = 0, actorToken = 0;
void* world = nullptr;
bool cached = false, loadFails = false, spawnFails = false, prepareFails = false, refreshFails = false;
bool worldIsReady = true, badArguments = false;
unsigned finds = 0, loads = 0, spawns = 0, preparations = 0, refreshCalls = 0;
int searchFunction = 0, refreshFunction = 0;
unsigned hubFinds = 0, hubProcesses = 0;
uint16_t hubParameterSize = 0;
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
bool ready(void* value) { if (value != world) badArguments = true; return worldIsReady; }
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
void* findHubFunction(const nwi::WideView* value) {
    ++hubFinds;
    if (equal(value, nwi::ModHubSearchPath)) return &searchFunction;
    if (equal(value, nwi::ModHubRefreshPath)) return &refreshFunction;
    badArguments = true; return nullptr;
}
uint16_t* parameterSize(void*) { return &hubParameterSize; }
bool processHub(void* hub, void* function, void* parameters) {
    if (hub != &actorToken || parameters || (function != &searchFunction && function != &refreshFunction)) badArguments = true;
    if (hubProcesses == 0 && function != &searchFunction) badArguments = true;
    if (hubProcesses == 1 && function != &refreshFunction) badArguments = true;
    ++hubProcesses; return true;
}
void setup(nwi::PresentationBootstrap& p) {
    world = &worlds[0]; cached = loadFails = spawnFails = prepareFails = refreshFails = badArguments = false;
    worldIsReady = true;
    finds = loads = spawns = preparations = refreshCalls = 0;
    p.configure({find, load, valid, spawn, currentWorld, worldKind, ready, prepare, refresh});
}
#define REQUIRE(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
}
int main() {
    {
        // Discovery alone is insufficient: the visible Mod Hub pages must be rebuilt afterwards.
        badArguments = false; hubFinds = hubProcesses = 0; hubParameterSize = 0;
        REQUIRE(nwi::rescanAndRefreshModHub({findHubFunction, valid, parameterSize, processHub}, &actorToken));
        REQUIRE(hubFinds == 2 && hubProcesses == 2 && !badArguments);
        hubFinds = hubProcesses = 0; hubParameterSize = 4;
        REQUIRE(!nwi::rescanAndRefreshModHub({findHubFunction, valid, parameterSize, processHub}, &actorToken));
        REQUIRE(hubFinds == 1 && hubProcesses == 0);
    }
    {
        nwi::PresentationBootstrap p; setup(p); world = &worlds[10];
        p.tick();
        REQUIRE(p.kind == nwi::WorldKind::SpaceRig && loads == 1 && preparations == 1 && spawns == 1 && refreshCalls == 1 && !badArguments);
        // A preloaded class from an integration entry still receives the native binding on mission travel.
        world = &worlds[0]; p.tick();
        REQUIRE(p.kind == nwi::WorldKind::Mission && loads == 1 && preparations == 2 && spawns == 2 && !badArguments);
    }
    {
        // Mod Hub may not exist on the first post-spawn callback; retry without duplicating Init.
        nwi::PresentationBootstrap p; setup(p); refreshFails = true;
        p.tick();
        REQUIRE(p.status == 7 && spawns == 1 && refreshCalls == 1);
        refreshFails = false; p.tick();
        REQUIRE(p.status == 3 && p.refreshes == 1 && p.refreshFailures == 1 && refreshCalls == 2 && spawns == 1);
    }
    {
        // A permanently absent Mod Hub has a strict retry cap and never becomes an actor-spawn loop.
        nwi::PresentationBootstrap p; setup(p); refreshFails = true;
        p.tick();
        for (unsigned i = 1; i < nwi::PresentationBootstrap::MaxRefreshAttempts; ++i) p.tick();
        REQUIRE(p.status == 8 && refreshCalls == nwi::PresentationBootstrap::MaxRefreshAttempts && spawns == 1);
        for (int i = 0; i < 100; ++i) p.tick();
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
        for (int i = 0; i < 100; ++i) p.tick();
        REQUIRE(p.status == 6 && !spawns && !loads && p.excluded == 1 && !p.attempted);
        world = &worlds[0]; p.tick();
        REQUIRE(spawns == 1 && p.status == 3);
        world = &worlds[11]; p.tick();
        world = &worlds[0]; p.tick();
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
        nwi::PresentationBootstrap p; setup(p); worldIsReady = false;
        for (int i = 0; i < 100; ++i) p.tick();
        REQUIRE(finds == 0 && spawns == 0 && p.status == 2);
        worldIsReady = true; p.tick();
        REQUIRE(spawns == 1 && loads == 1 && p.status == 3);
        for (int i = 0; i < 100; ++i) p.tick();
        REQUIRE(spawns == 1 && !badArguments);
        // Class pointers are reacquired, not retained across a world change/GC.
        world = &worlds[1]; cached = false; p.tick();
        REQUIRE(spawns == 2 && loads == 2 && p.spawned == 2);
        for (int i = 2; i < 12; ++i) {
            world = &worlds[i]; p.tick();
        }
        REQUIRE(p.status == 5 && p.attempted == 8 && spawns == 8 && !badArguments);
    }
    for (int failure = 0; failure < 2; ++failure) {
        nwi::PresentationBootstrap p; setup(p); loadFails = failure == 0; prepareFails = failure == 1;
        for (unsigned i = 0; i < nwi::PresentationBootstrap::MaxPreparationAttempts; ++i) p.tick();
        REQUIRE(p.status == 4 && p.preparationFailures == nwi::PresentationBootstrap::MaxPreparationAttempts);
        for (int i = 0; i < 100; ++i) p.tick();
        REQUIRE(loads == static_cast<unsigned>(failure == 0 ? nwi::PresentationBootstrap::MaxPreparationAttempts : 1)
            && spawns == 0 && !badArguments);
    }
    {
        nwi::PresentationBootstrap p; setup(p); spawnFails = true; p.tick();
        REQUIRE(p.status == 4 && spawns == 1 && !badArguments);
    }
    {
        // A null travel World does not poison the next ready gameplay World.
        nwi::PresentationBootstrap p; setup(p); world = nullptr; p.tick();
        world = &worlds[0]; worldIsReady = false; p.tick(); REQUIRE(spawns == 0);
        worldIsReady = true; p.tick(); REQUIRE(spawns == 1 && !badArguments);
    }
    puts("PASS: readiness-driven startup, bounded preparation/Mod Hub retries, null travel world, bridge fault stop, exact active-world bootstrap, reuse and creation limits.");
}
