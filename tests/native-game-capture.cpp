// Real MinHook trampoline/ABI harness against synthetic game-like functions; never loads the installed game.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <intrin.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <thread>
#include "../mods/NormalWaveNativeProbe/GameCapture.h"
#include "../mods/NormalWaveNativeProbe/OriginRegions.h"
namespace {
alignas(16) unsigned char owner[0x300]{};
alignas(16) unsigned char normalContext[0x100]{}; // The actual normal entry receives a component, NOT UWorld.
int worldToken = 0, pawnToken = 0;
struct Item { unsigned char bytes[128]{}; };
std::vector<Item> queue;
uintptr_t normalReturn = 0, enqueueReturn = 0, actorReturn = 0, shrinkReturn = 0;
uintptr_t batchReturn = 0, middleReturn = 0, outerReturn = 0;
uint64_t normalCalls = 0, enqueueCalls = 0, actorCalls = 0, shrinkCalls = 0;
volatile uint32_t afterCall = 0;
bool badArguments = false;
bool bypassMiddle = false; // Same normal entry but an unproven downstream route must still be rejected.
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
__declspec(noinline) void middle() { middleReturn = reinterpret_cast<uintptr_t>(_ReturnAddress()); batch(); ++afterCall; }
__declspec(noinline) void outer() { outerReturn = reinterpret_cast<uintptr_t>(_ReturnAddress()); middle(); ++afterCall; }
__declspec(noinline) void normal(void* context, float difficulty, void* locations, bool a, bool b) {
    normalReturn = reinterpret_cast<uintptr_t>(_ReturnAddress()); ++normalCalls;
    if (context != normalContext || difficulty != 1.75f || !locations || !a || b) badArguments = true;
    if (bypassMiddle) batch(); else outer(); ++afterCall;
}
__declspec(noinline) void callNormal() { int locations = 1; normal(normalContext, 1.75f, &locations, true, false); ++afterCall; }
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
}
int main() {
    using namespace nwi;
    auto* world = &worldToken; memcpy(owner + 0xa8, &world, 8);
    memcpy(normalContext + 0xa8, &world, 8);
    callNormal(); consume(); // Capture exact harness call sites before enabling actual trampolines.
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
    binding.sourceChain = {enqueueReturn, batchReturn, middleReturn, outerReturn, normalReturn};
    REQUIRE(capture::install(binding)); capture::setWorld(world);
    SpawnEvent event;
    void* absent = nullptr;
    memcpy(normalContext + 0xa8, &absent, 8);
    callNormal(); consume(); REQUIRE(!capture::stats().waves && !capture::pop(event));
    void* otherWorld = &pawnToken;
    memcpy(normalContext + 0xa8, &otherWorld, 8);
    callNormal(); consume(); REQUIRE(!capture::stats().waves && !capture::pop(event));
    memcpy(normalContext + 0xa8, &world, 8);
    capture::setWorld(nullptr); // Rig/loading has no active mission identity even if a matching context exists.
    callNormal(); consume(); REQUIRE(!capture::stats().waves && !capture::pop(event));
    capture::setWorld(world);
    batch(); // Preexisting event queue: attach as unknown when the first normal wave arrives.
    const auto baselineEnqueue = enqueueCalls, baselineActor = actorCalls;
    callNormal(); consume(); capture::poll(1);
    int emitted = 0;
    while (capture::pop(event)) { REQUIRE(event.wave == 1 && event.pawn && event.origin.descriptor != 3); ++emitted; }
    REQUIRE(emitted == 2 && enqueueCalls == baselineEnqueue + 3 && actorCalls == baselineActor + 6);
    REQUIRE(!badArguments && !capture::stats().fault);
    puts("PASS: distinct context/manager/World; null/wrong/inactive worlds rejected; actual five-frame unwind crosses enabled native trampolines.");
    batch(); consume(); REQUIRE(!capture::pop(event)); // Later scripted/egg-like source is also not normal.

    LARGE_INTEGER start{}, end{}, frequency{}; QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&start);
    const auto startNormal = normalCalls, startEnqueue = enqueueCalls, startActor = actorCalls, startShrink = shrinkCalls;
    for (uint64_t i = 0; i < 10000; ++i) {
        callNormal(); consume(); capture::poll(i + 2);
        int count = 0; while (capture::pop(event)) { ++count; REQUIRE(event.wave == i + 2); }
        REQUIRE(count == 2 && !capture::stats().fault);
    }
    REQUIRE(normalCalls - startNormal == 10000 && enqueueCalls - startEnqueue == 30000);
    REQUIRE(actorCalls - startActor == 30000 && shrinkCalls - startShrink == 30000 && !badArguments);
    QueryPerformanceCounter(&end);
    printf("PASS: 10000 hooked waves, originals called exactly once, float/bool/pointer ABI, failed actors, unknown baseline, swap removal/reallocation; synthetic elapsed=%.3f ms.\n", 1000.0 * (end.QuadPart - start.QuadPart) / frequency.QuadPart);
    const auto taggedBeforeAlternate = capture::stats().tagged;
    bypassMiddle = true; callNormal(); consume(); bypassMiddle = false;
    REQUIRE(!capture::pop(event) && capture::stats().tagged == taggedBeforeAlternate && capture::stats().sourceRejected == 3);
    REQUIRE(capture::stats().rejectedSourceFrames[0] && !capture::stats().fault);
    const auto wavesBeforeThread = capture::stats().waves;
    std::thread foreign([] { callNormal(); consume(); }); foreign.join();
    REQUIRE(capture::stats().waves == wavesBeforeThread && !capture::stats().fault && !capture::pop(event) && !badArguments);
    puts("PASS: real alternate stack route and foreign-thread calls never gain normal attribution; rejection stores bounded RVAs.");
    callNormal(); queue.clear(); sync(); // A missed destructive mutation disables observations before new tags.
    callNormal(); consume(); REQUIRE(capture::stats().fault && !capture::pop(event));
    REQUIRE(!badArguments);

    capture::Binding proof; proof.imageBase = 100; proof.imageEnd = 1000;
    proof.sourceChain = {110, 120, 130, 140, 150}; proof.enqueueReturns[1] = 111;
    uintptr_t stack[] = {2000, 110, 120, 130, 140, 5000, 150};
    REQUIRE(capture::sourceMatches(stack, 7, proof)); stack[1] = 111;
    REQUIRE(capture::sourceMatches(stack, 7, proof)); stack[3] = 131;
    REQUIRE(!capture::sourceMatches(stack, 7, proof));

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
    capture::setWorld(nullptr); capture::setWorld(world); // Retire a healthy epoch, not one already disabled by a fault.
    callNormal(); consume(); while (capture::pop(event)) {}
    REQUIRE(!capture::stats().fault);
    const auto wavesBeforeStop = capture::stats().waves, normalBeforeStop = normalCalls;
    std::thread retire([] { capture::stop(); }); retire.join();
    callNormal(); consume();
    REQUIRE(capture::stats().waves == wavesBeforeStop && normalCalls == normalBeforeStop + 1 && !badArguments);
    puts("PASS: cross-thread stop preserves original gameplay forwarding and prevents new observations.");
}
