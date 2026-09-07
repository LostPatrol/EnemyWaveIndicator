// Synchronous, bounded native observations. No UE calls, logging, heap allocation or gameplay retries in hooks.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <intrin.h>
#include <cstring>
#include <atomic>
#include "GameCapture.h"
#include "../../third_party/MinHook/include/MinHook.h"

namespace nwi::capture {
namespace {
using Normal = void (*)(void*, float, void*, bool, bool);
using Enqueue = bool (*)(void*, void*, const void*, const void*, bool, bool);
using Actor = void* (*)(void*, void*, const void*, const void*);
using Shrink = void (*)(void*);
Normal originalNormal = nullptr; Enqueue originalEnqueue = nullptr;
Actor originalActor = nullptr; Shrink originalShrink = nullptr;
Binding binding; Stats counters; SpawnAttribution ledger;
void* world = nullptr; void* manager = nullptr;
uint64_t epoch = 0, frame = 0, scopeWave = 0; void* scopeWorld = nullptr;
SpawnKey scopeCenter{};
uint64_t scopeSelectedMs = 0;
bool installed = false;
std::atomic<bool> enabled{false}; // Loader-thread retirement never mutates the game-thread ledger.
int64_t qpcFrequency = 0;
struct Array { unsigned char* data; int32_t count, capacity; };
struct Pending { SpawnKey key{}; uint32_t index = 0; uint64_t pawn = 0, capturedMs = 0; int64_t capturedQpc = 0; bool active = false; } pending;
static_assert(sizeof(Array) == 16);

bool onThread() noexcept { return GetCurrentThreadId() == binding.threadId; }
void fail(uint32_t reason) noexcept { if (!counters.fault) counters.fault = reason; ledger.stop(); pending = {}; }
// Contain only observer reads. The original game function is NEVER inside this exception handler.
bool readArray(void* owner, Array& out) noexcept {
    __try { memcpy(&out, static_cast<unsigned char*>(owner) + 0x1c0, sizeof(out)); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    return out.count >= 0 && out.count <= static_cast<int32_t>(SpawnAttribution::Capacity)
        && out.capacity >= out.count && out.capacity <= 65536 && (!out.count || out.data);
}
bool readKey(const Array& array, uint32_t index, SpawnKey& key) noexcept {
    if (index >= static_cast<uint32_t>(array.count)) return false;
    __try {
        const auto* item = array.data + index * 128;
        memcpy(&key.descriptor, item + 0x28, 8);
        memcpy(&key.x, item + 0x50, 12);
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    return key.valid();
}
// Both entry context and spawn manager are registered UActorComponents. +0xA8 is WorldPrivate,
// as independently used by game code at 16ab6c8/1657202. Never equate the component pointer to UWorld.
bool componentWorld(void* owner) noexcept {
    __try { return *reinterpret_cast<void**>(static_cast<unsigned char*>(owner) + 0xa8) == world; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool attach(void* owner) noexcept {
    Array array{}; std::array<SpawnKey, SpawnAttribution::Capacity> keys{};
    if (!componentWorld(owner) || !readArray(owner, array)) return false;
    for (int32_t i = 0; i < array.count; ++i) if (!readKey(array, static_cast<uint32_t>(i), keys[i])) return false;
    if (!ledger.attach(++epoch, reinterpret_cast<uint64_t>(owner), keys.data(), array.count)) return false;
    manager = owner; return true;
}
bool provenance(uintptr_t caller) noexcept {
    if (!scopeWave || scopeWorld != world || (caller != binding.enqueueReturns[0] && caller != binding.enqueueReturns[1])) return false;
    if (!binding.sourceChain[0]) return true; // Synthetic ABI harness; production always requires the full chain.
    void* frames[32]{};
    const auto count = CaptureStackBackTrace(0, 32, frames, nullptr);
    const bool matches = sourceMatches(reinterpret_cast<uintptr_t*>(frames), count, binding);
    if (!matches && !counters.rejectedSourceFrames[0]) {
        size_t stored = 0;
        for (USHORT i = 0; i < count && stored < counters.rejectedSourceFrames.size(); ++i) {
            const auto address = reinterpret_cast<uintptr_t>(frames[i]);
            if (address >= binding.imageBase && address < binding.imageEnd)
                counters.rejectedSourceFrames[stored++] = address - binding.imageBase;
        }
    }
    return matches;
}

// The audited natural selector passes exactly one FVector in this array. Never guess a center
// from enemy positions when a different caller supplies multiple centers or an invalid layout.
SpawnKey readCenter(void* locations) noexcept {
    SpawnKey result{};
    __try {
        const auto& array = *static_cast<Array*>(locations);
        if (array.count == 1 && array.capacity >= 1 && array.data) {
            memcpy(&result.x, array.data, 12); result.descriptor = 1;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { return {}; }
    return result.valid() ? result : SpawnKey{};
}

void hookNormal(void* context, float difficulty, void* locations, bool a, bool b) {
    const auto caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    bool observe = enabled.load(std::memory_order_relaxed) && onThread() && !counters.fault;
    if (observe) {
        ++counters.normalEntries;
        observe = caller == binding.normalReturn;
        if (observe) {
            ++counters.normalSiteMatches;
            if (!world) { ++counters.noMissionWorld; observe = false; }
            else if (!componentWorld(context)) { ++counters.contextRejected; observe = false; }
        }
    }
    if (!observe) { originalNormal(context, difficulty, locations, a, b); return; }
    const auto previousWave = scopeWave; auto* previousWorld = scopeWorld; const auto previousCenter = scopeCenter; const auto previousSelected = scopeSelectedMs;
    if (observe) { scopeWave = ++counters.waves; scopeWorld = world; scopeCenter = readCenter(locations); scopeSelectedMs = GetTickCount64(); }
    originalNormal(context, difficulty, locations, a, b);
    if (observe) { scopeWave = previousWave; scopeWorld = previousWorld; scopeCenter = previousCenter; scopeSelectedMs = previousSelected; }
}
bool hookEnqueue(void* owner, void* descriptor, const void* transform, const void* callback, bool a, bool b) {
    const auto caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    bool observe = enabled.load(std::memory_order_relaxed) && onThread() && !counters.fault && world;
    const bool normal = observe && provenance(caller);
    if (observe && scopeWave && !normal) ++counters.sourceRejected;
    if (observe && !manager && normal && !attach(owner)) fail(1);
    observe = observe && owner == manager && !counters.fault;
    Array before{};
    if (observe && (!readArray(owner, before) || before.count != static_cast<int32_t>(ledger.tracked()))) { fail(2); observe = false; }
    const bool result = originalEnqueue(owner, descriptor, transform, callback, a, b);
    if (observe && !counters.fault) {
        Array after{}; SpawnKey key{};
        if (!readArray(owner, after)) fail(3);
        else if (after.count == before.count) ++counters.skipped; // Accepted/rejected request with no actual append.
        else if (after.count != before.count + 1 || !readKey(after, before.count, key)
            || key.descriptor != reinterpret_cast<uint64_t>(descriptor)
            || !ledger.append(epoch, reinterpret_cast<uint64_t>(manager), before.count, after.count, key, normal ? scopeWave : 0,
                normal ? scopeCenter : SpawnKey{}, GetTickCount64(), normal ? scopeSelectedMs : 0)) fail(4);
        else if (normal) ++counters.tagged;
    }
    return result;
}
void* hookActor(void* context, void* cls, const void* transform, const void* parameters) {
    const auto caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    bool observe = enabled.load(std::memory_order_relaxed) && caller == binding.actorReturn && onThread() && !counters.fault && manager && context == world;
    if (observe) {
        Array array{};
        const auto address = reinterpret_cast<uintptr_t>(transform);
        if (pending.active || !readArray(manager, array) || array.count != static_cast<int32_t>(ledger.tracked())) { fail(5); observe = false; }
        else {
            const auto begin = reinterpret_cast<uintptr_t>(array.data) + 0x40;
            if (address < begin || (address - begin) % 128 || (address - begin) / 128 >= static_cast<uintptr_t>(array.count)) { fail(6); observe = false; }
            else {
                pending.index = static_cast<uint32_t>((address - begin) / 128);
                if (!readKey(array, pending.index, pending.key) || !ledger.matches(pending.index, pending.key)) { fail(7); observe = false; }
                else pending.active = true;
            }
        }
    }
    void* result = originalActor(context, cls, transform, parameters);
    if (observe && !counters.fault) {
        pending.pawn = reinterpret_cast<uint64_t>(result); pending.capturedMs = GetTickCount64();
        LARGE_INTEGER stamp{}; QueryPerformanceCounter(&stamp); pending.capturedQpc = stamp.QuadPart;
    }
    return result;
}
void hookShrink(void* arrayPointer) {
    const auto caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    const bool observe = enabled.load(std::memory_order_relaxed) && caller == binding.shrinkReturn && onThread() && !counters.fault && manager
        && arrayPointer == static_cast<unsigned char*>(manager) + 0x1c0;
    originalShrink(arrayPointer); // The game has already swapped/decremented; preserve its allocation policy.
    if (observe) {
        Array after{};
        if (!pending.active || !readArray(manager, after)
            || !ledger.removeSwap(epoch, reinterpret_cast<uint64_t>(manager), pending.index,
                static_cast<uint32_t>(after.count + 1), static_cast<uint32_t>(after.count), pending.key,
                pending.pawn, frame, pending.capturedMs, pending.capturedQpc)) fail(8);
        else {
            SpawnKey moved{};
            if (pending.index < static_cast<uint32_t>(after.count)
                && (!readKey(after, pending.index, moved) || !ledger.matches(pending.index, moved))) fail(9);
            else if (pending.pawn) ++counters.successes;
            else ++counters.failures;
        }
        pending = {};
    }
}
} // namespace

bool sourceMatches(const uintptr_t* frames, size_t count, const Binding& expected) noexcept {
    size_t matched = 0;
    for (size_t i = 0; i < count && matched < expected.sourceChain.size(); ++i) {
        const auto address = frames[i];
        if (address < expected.imageBase || address >= expected.imageEnd) continue;
        if (address != expected.sourceChain[matched] && !(matched == 0 && address == expected.enqueueReturns[1])) return false;
        ++matched;
        if (matched < expected.sourceChain.size() && !expected.sourceChain[matched]) return true; // Short synthetic fixtures only.
    }
    return matched == expected.sourceChain.size();
}
bool install(const Binding& input) noexcept {
    if (installed) return true;
    binding = input;
    if (!onThread()) return false;
    LARGE_INTEGER frequency{}; QueryPerformanceFrequency(&frequency); qpcFrequency = frequency.QuadPart;
    const Target targets[] = {binding.normal, binding.enqueue, binding.actor, binding.shrink};
    for (const auto& target : targets) if (!target.address || memcmp(target.address, target.bytes.data(), target.bytes.size())) { counters.hookStatus = 100; return false; }
    auto status = MH_Initialize();
    if (status != MH_OK) { counters.hookStatus = static_cast<uint32_t>(status) + 1; return false; }
    void* replacements[] = {reinterpret_cast<void*>(&hookNormal), reinterpret_cast<void*>(&hookEnqueue), reinterpret_cast<void*>(&hookActor), reinterpret_cast<void*>(&hookShrink)};
    void** originals[] = {reinterpret_cast<void**>(&originalNormal), reinterpret_cast<void**>(&originalEnqueue), reinterpret_cast<void**>(&originalActor), reinterpret_cast<void**>(&originalShrink)};
    for (size_t i = 0; i < 4; ++i) {
        status = MH_CreateHook(targets[i].address, replacements[i], originals[i]);
        if (status != MH_OK) { counters.hookStatus = static_cast<uint32_t>(status) + 1; return false; }
    }
    status = MH_EnableHook(MH_ALL_HOOKS); // One suspension pass; no code changes during spawn observations.
    if (status != MH_OK) { MH_DisableHook(MH_ALL_HOOKS); counters.hookStatus = static_cast<uint32_t>(status) + 1; return false; }
    installed = true; enabled.store(true); counters.hookStatus = 1; return true;
}
void setWorld(void* value) noexcept {
    if (!onThread() || world == value) return;
    world = value; manager = nullptr; scopeWave = 0; scopeWorld = nullptr; pending = {};
    ledger.stop(); counters.fault = 0;
}
void stop() noexcept { enabled.store(false); }
void poll(uint64_t value) noexcept { if (onThread()) { frame = value; ++counters.polls; } }
bool pop(SpawnEvent& event) noexcept {
    if (!enabled.load(std::memory_order_relaxed) || !onThread() || counters.fault || !manager) return false;
    if (!ledger.pop(epoch, frame, 2, event)) return false;
    ++counters.delivered;
    const auto latency = GetTickCount64() - event.capturedMs;
    if (latency > counters.deliveryMaxMs) counters.deliveryMaxMs = latency;
    LARGE_INTEGER stamp{}; QueryPerformanceCounter(&stamp);
    if (qpcFrequency > 0 && event.capturedQpc > 0 && stamp.QuadPart >= event.capturedQpc) {
        const auto microseconds = static_cast<uint64_t>((stamp.QuadPart - event.capturedQpc) * 1000000 / qpcFrequency);
        if (microseconds > counters.deliveryMaxUs) counters.deliveryMaxUs = microseconds;
    }
    return true;
}
Stats stats() noexcept { return counters; } // Caller is the game thread; publish through DispatchProbe's snapshot.
void* spawnManager() noexcept { return onThread() ? manager : nullptr; }
} // namespace nwi::capture
