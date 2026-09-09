// Real MinHook trampoline/ABI harness against synthetic game-like functions; never loads the installed game.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <intrin.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <thread>
#include "../mods/EnemyWaveNativeProbe/GameCapture.h"
#include "../mods/EnemyWaveNativeProbe/OriginRegions.h"
#include "../mods/EnemyWaveNativeProbe/EnemyBuckets.h"
namespace {
alignas(16) unsigned char owner[0x300]{};
alignas(16) unsigned char normalContext[0x100]{}; // The actual normal entry receives a component, NOT UWorld.
int worldToken = 0, pawnToken = 0;
struct Item { unsigned char bytes[128]{}; };
std::vector<Item> queue;
uintptr_t normalReturn = 0, enqueueReturn = 0, actorReturn = 0, shrinkReturn = 0;
uintptr_t schedulerReturn = 0;
uintptr_t batchReturn = 0, middleReturn = 0, outerReturn = 0;
uint64_t normalCalls = 0, enqueueCalls = 0, actorCalls = 0, shrinkCalls = 0;
volatile uint32_t afterCall = 0;
bool badArguments = false;
bool bypassMiddle = false; // Same normal entry but an unproven downstream route must still be rejected.
uint64_t sourceCalls = 0, centerCalls = 0;
void sync() {
    struct Array { void* data; int32_t count, capacity; } array{queue.data(), static_cast<int32_t>(queue.size()), static_cast<int32_t>(queue.capacity())};
    memcpy(owner + 0x1c0, &array, sizeof(array));
}
// Separate PE section models game code versus the observer DLL, enabling real stack unwinding in this harness.
#pragma code_seg(push, ".fixture")
__declspec(noinline) bool enqueue(void* manager, void* descriptor, const void* transform, const void* callback, bool fx, bool alert) {
    enqueueReturn = reinterpret_cast<uintptr_t>(_ReturnAddress()); ++enqueueCalls;
    if (manager != owner || !callback || !fx || alert) badArguments = true;
    if (!descriptor) return true; // A true result without append must never be tagged.
    Item item; memcpy(item.bytes + 0x28, &descriptor, 8); memcpy(item.bytes + 0x40, transform, 48);
    queue.push_back(item); sync(); return true;
}
__declspec(noinline) void batch() {
    batchReturn = reinterpret_cast<uintptr_t>(_ReturnAddress());
    alignas(16) float transform[12]{}; uint64_t callback[2]{};
    transform[4] = 10; transform[5] = 20; transform[6] = 30;
    for (uintptr_t descriptor = 1; descriptor <= 3; ++descriptor) {
        if (!enqueue(owner, reinterpret_cast<void*>(descriptor), transform, callback, true, false)) badArguments = true;
        ++afterCall;
    }
}
// Exact ABI of the six additional native entrypoints, with two game-selected centers per request.
__declspec(noinline) void sourceCenter(void* context, void* descriptor, int32_t count, float weight, const void* point, const void* callback, uint8_t pf, const void* settings, bool flag) {
    ++centerCalls;
    if (context != &worldToken || descriptor != reinterpret_cast<void*>(91) || count != 3 || weight != 2.25f || !point || callback != reinterpret_cast<void*>(92) || pf != 2 || settings != reinterpret_cast<void*>(93) || flag) badArguments = true;
    const auto* xyz=static_cast<const float*>(point);
    if ((xyz[0]!=100 && xyz[0]!=101) || xyz[1]!=200 || xyz[2]!=300) badArguments=true;
    batch(); ++afterCall;
}
void sourceBatch() {
    float point[3]{100,200,300};
    sourceCenter(&worldToken,reinterpret_cast<void*>(91),3,2.25f,point,reinterpret_cast<void*>(92),2,reinterpret_cast<void*>(93),false);
    point[0]=101; // One centimeter separation proves the actual centers are not proximity-merged.
    sourceCenter(&worldToken,reinterpret_cast<void*>(91),3,2.25f,point,reinterpret_cast<void*>(92),2,reinterpret_cast<void*>(93),false);
}
__declspec(noinline) void sourcePool(void*, float difficulty, void* locations, void* banned, bool alert, bool pressure) {
    ++sourceCalls; if(difficulty!=7.25f || locations!=reinterpret_cast<void*>(81) || banned!=reinterpret_cast<void*>(82) || !alert || pressure) badArguments=true;
    sourceBatch(); ++afterCall;
}
__declspec(noinline) void sourceLocation(void*, void* desc, int32_t count, const void* point, const void* callback, bool alert, bool scale, uint8_t pf) {
    ++sourceCalls; if(desc!=reinterpret_cast<void*>(81) || count!=7 || !point || callback!=reinterpret_cast<void*>(82) || !alert || scale || pf!=2) badArguments=true;
    sourceBatch(); ++afterCall;
}
__declspec(noinline) void sourceGroup(void*, void* desc, float difficulty, const void* point, bool alert, uint8_t pf) {
    ++sourceCalls; if(desc!=reinterpret_cast<void*>(81) || difficulty!=7.25f || !point || !alert || pf!=2) badArguments=true;
    sourceBatch(); ++afterCall;
}
__declspec(noinline) void sourceSpread(void*, void* desc, float difficulty, const void* points, bool alert, uint8_t pf) {
    ++sourceCalls; if(desc!=reinterpret_cast<void*>(81) || difficulty!=7.25f || !points || !alert || pf!=2) badArguments=true;
    sourceBatch(); ++afterCall;
}
__declspec(noinline) void sourceCallback(void*, void* desc, float difficulty, const void* points, bool alert, uint8_t pf, const void* callback) {
    ++sourceCalls; if(desc!=reinterpret_cast<void*>(81) || difficulty!=7.25f || !points || !alert || pf!=2 || callback!=reinterpret_cast<void*>(82)) badArguments=true;
    sourceBatch(); ++afterCall;
}
__declspec(noinline) void middle() { middleReturn = reinterpret_cast<uintptr_t>(_ReturnAddress()); batch(); ++afterCall; }
__declspec(noinline) void outer() { outerReturn = reinterpret_cast<uintptr_t>(_ReturnAddress()); middle(); ++afterCall; }
__declspec(noinline) void normal(void* context, float difficulty, void* locations, bool a, bool b) {
    normalReturn = reinterpret_cast<uintptr_t>(_ReturnAddress()); ++normalCalls;
    if (context != normalContext || difficulty != 1.75f || !locations || !a || b) badArguments = true;
    struct Locations { float* data; int32_t count, capacity; };
    const auto* centers=static_cast<Locations*>(locations);
    if (centers->count!=1 || centers->capacity!=1 || centers->data[0]!=500 || centers->data[1]!=600 || centers->data[2]!=700) badArguments=true;
    if (bypassMiddle) batch(); else outer(); ++afterCall;
}
__declspec(noinline) void callNormal() {
    schedulerReturn=reinterpret_cast<uintptr_t>(_ReturnAddress());
    float center[3]{500,600,700}; struct { void* data; int32_t count, capacity; } locations{center,1,1};
    normal(normalContext, 1.75f, &locations, true, false); ++afterCall;
}
__declspec(noinline) void scheduleNormal() { callNormal(); ++afterCall; }
__declspec(noinline) void* actor(void* world, void* cls, const void* transform, const void* params) {
    actorReturn = reinterpret_cast<uintptr_t>(_ReturnAddress()); ++actorCalls;
    if (world != &worldToken || !transform || !params) badArguments = true;
    return cls == reinterpret_cast<void*>(3) ? nullptr : &pawnToken;
}
__declspec(noinline) void shrink(void* array) {
    shrinkReturn = reinterpret_cast<uintptr_t>(_ReturnAddress()); ++shrinkCalls;
    if (array != owner + 0x1c0) badArguments = true;
    if (queue.empty()) queue.shrink_to_fit(); sync();
}
__declspec(noinline) void consume() {
    // Remove front deliberately: every step moves the last item, unlike an easy FIFO-only test.
    while (!queue.empty()) {
        void* descriptor; memcpy(&descriptor, queue[0].bytes + 0x28, 8);
        uint64_t params[8]{};
        actor(&worldToken, descriptor, queue[0].bytes + 0x40, params);
        queue[0] = queue.back(); queue.pop_back(); sync();
        shrink(owner + 0x1c0); ++afterCall;
    }
}
#pragma code_seg(pop)
template<class T> nwi::capture::Target target(T function) {
    nwi::capture::Target t; t.address = reinterpret_cast<void*>(function); memcpy(t.bytes.data(), t.address, t.bytes.size()); return t;
}
#define REQUIRE(x) do { if (!(x)) { printf("FAIL line %d: %s (fault=%u)\n", __LINE__, #x, nwi::capture::stats().fault); return 1; } } while (0)
int32_t classifySource(void* context, void* world) {
    const auto id = reinterpret_cast<uintptr_t>(context);
    return world == &worldToken && id > 0 && id < nwi::WaveTypeCount ? static_cast<int32_t>(id) : -1;
}
}
int main() {
    using namespace nwi;
    auto* world = &worldToken; memcpy(owner + 0xa8, &world, 8);
    memcpy(normalContext + 0xa8, &world, 8);
    scheduleNormal(); consume(); // Capture exact harness call sites before enabling actual trampolines.
    capture::Binding binding;
    binding.normal = target(normal); binding.enqueue = target(enqueue); binding.actor = target(actor); binding.shrink = target(shrink);
    binding.normalReturn = normalReturn; binding.enqueueReturns[0] = enqueueReturn;
    binding.actorReturn = actorReturn; binding.shrinkReturn = shrinkReturn; binding.threadId = GetCurrentThreadId();
    auto* image = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    auto* pe = reinterpret_cast<IMAGE_NT_HEADERS*>(image + dos->e_lfanew);
    for (WORD i = 0; i < pe->FileHeader.NumberOfSections; ++i) {
        const auto& section = IMAGE_FIRST_SECTION(pe)[i];
        if (!memcmp(section.Name, ".fixture", 8)) {
            binding.imageBase = reinterpret_cast<uintptr_t>(image) + section.VirtualAddress;
            binding.imageEnd = binding.imageBase + section.Misc.VirtualSize;
        }
    }
    REQUIRE(binding.imageBase && binding.imageEnd > binding.imageBase);
    binding.sourceChain = {enqueueReturn, batchReturn, middleReturn, outerReturn, normalReturn, schedulerReturn};
    binding.pool=target(sourcePool);binding.location=target(sourceLocation);binding.group=target(sourceGroup);
    binding.spread=target(sourceSpread);binding.spreadCallback=target(sourceCallback);binding.center=target(sourceCenter);binding.classifySource=classifySource;
    REQUIRE(capture::install(binding)); capture::setWorld(world);
    SpawnEvent event;
    void* absent = nullptr;
    memcpy(normalContext + 0xa8, &absent, 8);
    scheduleNormal(); consume(); REQUIRE(!capture::stats().waves && !capture::pop(event));
    void* otherWorld = &pawnToken;
    memcpy(normalContext + 0xa8, &otherWorld, 8);
    scheduleNormal(); consume(); REQUIRE(!capture::stats().waves && !capture::pop(event));
    memcpy(normalContext + 0xa8, &world, 8);
    capture::setWorld(nullptr); // Rig/loading has no active mission identity even if a matching context exists.
    scheduleNormal(); consume(); REQUIRE(!capture::stats().waves && !capture::pop(event));
    capture::setWorld(world);
    batch(); // Preexisting event queue: attach as unknown when the first normal wave arrives.
    const auto baselineEnqueue = enqueueCalls, baselineActor = actorCalls;
    scheduleNormal(); consume(); capture::poll(1);
    int emitted = 0;
    while (capture::pop(event)) { REQUIRE(event.wave == 1 && event.pawn && event.origin.descriptor != 3 && event.center.valid() && event.center.x==500 && event.center.y==600 && event.center.z==700 && event.queuedMs<=event.capturedMs); ++emitted; }
    REQUIRE(emitted == 2 && enqueueCalls == baselineEnqueue + 3 && actorCalls == baselineActor + 6);
    REQUIRE(!badArguments && !capture::stats().fault);
    puts("PASS: distinct context/manager/World; null/wrong/inactive worlds rejected; actual six-frame unwind crosses enabled native trampolines.");
    batch(); consume(); REQUIRE(!capture::pop(event)); // Later scripted/egg-like source is also not normal.

    LARGE_INTEGER start{}, end{}, frequency{}; QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&start);
    const auto startNormal = normalCalls, startEnqueue = enqueueCalls, startActor = actorCalls, startShrink = shrinkCalls;
    for (uint64_t i = 0; i < 10000; ++i) {
        scheduleNormal(); consume(); capture::poll(i + 2);
        int count = 0; while (capture::pop(event)) { ++count; REQUIRE(event.wave == i + 2); }
        REQUIRE(count == 2 && !capture::stats().fault);
    }
    REQUIRE(normalCalls - startNormal == 10000 && enqueueCalls - startEnqueue == 30000);
    REQUIRE(actorCalls - startActor == 30000 && shrinkCalls - startShrink == 30000 && !badArguments);
    QueryPerformanceCounter(&end);
    printf("PASS: 10000 hooked waves, originals called exactly once, float/bool/pointer ABI, failed actors, unknown baseline, swap removal/reallocation; synthetic elapsed=%.3f ms.\n", 1000.0 * (end.QuadPart - start.QuadPart) / frequency.QuadPart);
    const auto taggedBeforeAlternate = capture::stats().tagged;
    bypassMiddle = true; scheduleNormal(); consume(); bypassMiddle = false;
    REQUIRE(!capture::pop(event) && capture::stats().tagged == taggedBeforeAlternate && capture::stats().sourceRejected == 3);
    REQUIRE(capture::stats().rejectedSourceFrames[0] && !capture::stats().fault);
    const auto manualTags=capture::stats().tagged;
    callNormal(); consume(); REQUIRE(!capture::pop(event) && capture::stats().tagged==manualTags); // Shared helper without scheduler is not natural.
    const auto wavesBeforeThread = capture::stats().waves;
    std::thread foreign([] { scheduleNormal(); consume(); }); foreign.join();
    REQUIRE(capture::stats().waves == wavesBeforeThread && !capture::stats().fault && !capture::pop(event) && !badArguments);
    puts("PASS: real alternate stack route and foreign-thread calls never gain normal attribution; rejection stores bounded RVAs.");
    scheduleNormal(); queue.clear(); sync(); // A missed destructive mutation disables observations before new tags.
    scheduleNormal(); consume(); REQUIRE(capture::stats().fault && !capture::pop(event));
    REQUIRE(!badArguments);

    capture::Binding proof; proof.imageBase = 100; proof.imageEnd = 1000;
    proof.sourceChain = {110, 120, 130, 140, 150, 160}; proof.enqueueReturns[1] = 111;
    uintptr_t stack[] = {2000, 110, 120, 130, 140, 5000, 150, 160};
    REQUIRE(capture::sourceMatches(stack, 8, proof)); stack[1] = 111;
    REQUIRE(capture::sourceMatches(stack, 8, proof)); stack[3] = 131;
    REQUIRE(!capture::sourceMatches(stack, 8, proof));

    OriginRegions regions; SpawnEvent sample{1, 1, 1, 1, {10, 0, 0, 0}, 100};
    REQUIRE(regions.add(sample, 100)); sample.origin.x = 10; REQUIRE(regions.add(sample, 101));
    REQUIRE(regions.items[0].point.x == 0 && regions.items[0].serial == 2);
    for (uint32_t i = 1; i < 8; ++i) { sample.origin.x = i * 1000.0f; REQUIRE(regions.add(sample, 102)); }
    sample.origin.x = 9000; REQUIRE(!regions.add(sample, 103) && regions.overflow == 1);
    REQUIRE(!regions.add(sample, 400) && regions.stale == 1);
    regions.expire(8200); for (const auto& r : regions.items) REQUIRE(!r.visible);
    REQUIRE(regions.add({1,2,1,2,{10,0,0,0},8200},8200));
    regions.reset(); regions.lifetimeMs = 12000;
    REQUIRE(regions.add({1,2,1,2,{10,0,0,0},9000},9000));
    regions.expire(17001); REQUIRE(regions.items[0].visible);
    regions.expire(21000); REQUIRE(!regions.items[0].visible);
    puts("PASS: strict source chain, eight reusable exact-origin regions, coalescing, overflow, expiry and late-event rejection.");
    regions.reset();
    SpawnEvent centered{1,7,1,1,{10,10000,0,0},100,0,{1,500,600,700},50};
    REQUIRE(regions.add(centered,100,90));centered.origin.x=-10000;REQUIRE(regions.add(centered,101,10));
    REQUIRE(regions.items[0].point.x==500 && regions.items[0].count==2 && regions.items[0].weight==100);
    const auto oldScale=regions.items[0].scale();REQUIRE(regions.add(centered,102,200) && regions.items[0].scale()>oldScale);
    centered.center.x=501;REQUIRE(regions.add(centered,103,10) && regions.items[1].point.x==501); // Distinct actual centers never merge by radius.
    void* listed[]={&pawnToken}; EnemyBucket regular{listed,1,1}, small{listed,1,1}, empty{};
    const auto pawn=reinterpret_cast<uint64_t>(&pawnToken);
    REQUIRE(eligibleEnemy(pawn,regular,empty,empty));
    REQUIRE(!eligibleEnemy(pawn,empty,empty,empty) && !eligibleEnemy(0,regular,empty,empty));
    REQUIRE(!eligibleEnemy(pawn,empty,small,empty) && eligibleScriptedEnemy(pawn,empty,small,empty));
    REQUIRE(!eligibleScriptedEnemy(pawn,regular,small,small) && !eligibleScriptedEnemy(pawn,empty,empty,empty));
    REQUIRE(!eligibleEnemy(pawn,regular,empty,small));
    const auto originalCount=regions.items[0].count; const auto originalExpiry=regions.items[0].expires;
    for (unsigned i=0;i<100;++i) if (eligibleEnemy(pawn,regular,small,empty)) regions.add(centered,104,90);
    REQUIRE(regions.items[0].count==originalCount && regions.items[0].expires==originalExpiry);
    small.count=-1;REQUIRE(!eligibleEnemy(pawn,regular,small,empty));
    puts("PASS: registered regular required; small/critter wins mixed membership; 100 excluded notifications cannot grow or renew markers.");
    // A new controller instance is a new mission epoch even when Unreal reuses UWorld's address.
    capture::setWorld(world);
    REQUIRE(capture::spawnManager() == nullptr);
    capture::poll(20000); scheduleNormal(); consume();
    emitted = 0; while (capture::pop(event)) ++emitted;
    REQUIRE(emitted == 2 && !capture::stats().fault);
    puts("PASS: same-address UWorld reuse resets the old manager ledger and accepts the next mission.");
    capture::setWorld(nullptr); capture::setWorld(world); // Retire a healthy epoch, not one already disabled by a fault.
    for (uint32_t type=1;type<WaveTypeCount;++type) {
        const auto priorCalls=sourceCalls; const auto priorCenters=centerCalls;
        sourcePool(reinterpret_cast<void*>(uintptr_t(type)),7.25f,reinterpret_cast<void*>(81),reinterpret_cast<void*>(82),true,false);
        consume(); uint32_t delivered=0;
        while(capture::pop(event)) { if(waveType(event.wave)!=type || !event.center.valid() || (event.center.x!=100 && event.center.x!=101)) printf("source expected=%u got=%u center=%f,%f,%f descriptor=%llu\n",type,waveType(event.wave),event.center.x,event.center.y,event.center.z,event.center.descriptor); REQUIRE(waveType(event.wave)==type && event.center.valid() && (event.center.x==100 || event.center.x==101)); ++delivered; }
        REQUIRE(delivered==4 && sourceCalls==priorCalls+1 && centerCalls==priorCenters+2 && !badArguments);
    }
    float point[3]{100,200,300};
    sourceLocation(reinterpret_cast<void*>(1),reinterpret_cast<void*>(81),7,point,reinterpret_cast<void*>(82),true,false,2);
    sourceGroup(reinterpret_cast<void*>(2),reinterpret_cast<void*>(81),7.25f,point,true,2);
    sourceSpread(reinterpret_cast<void*>(3),reinterpret_cast<void*>(81),7.25f,point,true,2);
    sourceCallback(reinterpret_cast<void*>(4),reinterpret_cast<void*>(81),7.25f,point,true,2,reinterpret_cast<void*>(82));
    sourcePool(nullptr,7.25f,reinterpret_cast<void*>(81),reinterpret_cast<void*>(82),true,false); // Unknown source stays unknown despite the same enemy descriptors.
    consume(); uint32_t perType[5]{}; regions.reset();
    while(capture::pop(event)) { REQUIRE(waveType(event.wave)>=1 && waveType(event.wave)<=4); ++perType[waveType(event.wave)]; REQUIRE(regions.add(event,GetTickCount64())); }
    for(uint32_t type=1;type<=4;++type) REQUIRE(perType[type]==4);
    for(const auto& region:regions.items) REQUIRE(region.visible && region.count==2);
    REQUIRE(!badArguments && !capture::stats().fault);
    puts("PASS: all 46 stock scripted types; ten real hooks; interleaved queued sources, unknown-descriptor isolation, exact multi-centers and callback/float/bool/pointer ABI.");
    scheduleNormal(); consume(); while (capture::pop(event)) {}
    REQUIRE(!capture::stats().fault);
    const auto wavesBeforeStop = capture::stats().waves, normalBeforeStop = normalCalls;
    std::thread retire([] { capture::stop(); }); retire.join();
    scheduleNormal(); consume();
    REQUIRE(capture::stats().waves == wavesBeforeStop && normalCalls == normalBeforeStop + 1 && !badArguments);
    puts("PASS: cross-thread stop preserves original gameplay forwarding and prevents new observations.");
}
