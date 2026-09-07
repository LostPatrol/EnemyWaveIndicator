// Host-only game-thread handoff. All pointer lookup is one-time per controller; updates touch only our own fields.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstring>
#include <cwchar>
#include <atomic>
#include "AutomaticPresentation.h"
#include "PresentationBootstrap.h"
#include "GameCapture.h"
#include "OriginRegions.h"
#include "EnemyBuckets.h"

namespace nwi::automatic {
namespace {
using NativeFunction = void (*)(void*, void*, void*);
using Find = void* (*)(const WideView*);
using Value = void* (*)(void*, const wchar_t*);
using GetFunc = NativeFunction* (*)(void*);
using GetFlags = uint32_t* (*)(void*);
using GetSize = uint16_t* (*)(void*);
using GetWorld = void* (*)(void*);
using GetName = size_t (*)(void*, wchar_t*, size_t);
Find find = nullptr; Value value = nullptr; GetFunc functionSlot = nullptr;
GetFlags flags = nullptr; GetSize parameters = nullptr; GetWorld actorWorld = nullptr; GetName name = nullptr;
uint32_t thread = 0; bool configured = false; Stats counters; OriginRegions regions;
void* activeController = nullptr; void* activeWorld = nullptr;
std::atomic<bool> retired{false};
int32_t* instanceCookie = nullptr;
float* durationSec = nullptr; // Own Blueprint property; never a game-class field.
float* worldTime = nullptr;
int32_t* enabledNatural = nullptr;
int32_t cookie = 0; // A fresh Blueprint instance has zero even when its allocator reuses an old address.
struct Output { float* point = nullptr; int32_t* serial = nullptr; int32_t* visible = nullptr; float* expires = nullptr; float* scale = nullptr; };
Output output[OriginRegions::Capacity]{};

template<class T> T resolve(HMODULE module, const char* symbol, uintptr_t rva) noexcept {
    auto raw = GetProcAddress(module, symbol); T result = nullptr;
    if (raw && reinterpret_cast<uintptr_t>(raw) - reinterpret_cast<uintptr_t>(module) == rva) memcpy(&result, &raw, sizeof(result));
    return result;
}
bool localAddress(void* actor, void* pointer, size_t bytes) noexcept {
    const auto start = reinterpret_cast<uintptr_t>(actor), at = reinterpret_cast<uintptr_t>(pointer);
    return at >= start + 0x100 && at + bytes <= start + 0x10000 && !(at % 4);
}
bool bind(void* actor) {
    auto* abi = static_cast<int32_t*>(value(actor, L"NativeAbi"));
    if (!localAddress(actor, abi, 4) || *abi != 0x80000) return false;
    worldTime = static_cast<float*>(value(actor, L"NativeTime"));
    enabledNatural = static_cast<int32_t*>(value(actor, L"NativeEnabled"));
    if (!localAddress(actor, worldTime, 4) || !localAddress(actor, enabledNatural, 4)) return false;
    durationSec = static_cast<float*>(value(actor, L"DurationSec"));
    if (!localAddress(actor, durationSec, 4)) return false;
    instanceCookie = static_cast<int32_t*>(value(actor, L"NativeCookie"));
    if (!localAddress(actor, instanceCookie, 4)) return false;
    cookie = cookie == INT32_MAX ? 1 : cookie + 1;
    *instanceCookie = cookie;
    auto* world = actorWorld(actor); wchar_t text[256]{};
    const auto length = name(world, text, 256);
    if (length >= 256 || classifyWorldName(text, length) == WorldKind::Excluded) return false;
    for (uint32_t i = 0; i < OriginRegions::Capacity; ++i) {
        wchar_t field[48];
        swprintf_s(field, L"RegionPoint%u", i); output[i].point = static_cast<float*>(value(actor, field));
        swprintf_s(field, L"RegionSerial%u", i); output[i].serial = static_cast<int32_t*>(value(actor, field));
        swprintf_s(field, L"RegionVisible%u", i); output[i].visible = static_cast<int32_t*>(value(actor, field));
        swprintf_s(field, L"RegionExpires%u", i); output[i].expires = static_cast<float*>(value(actor, field));
        swprintf_s(field, L"RegionScale%u", i); output[i].scale = static_cast<float*>(value(actor, field));
        if (!localAddress(actor, output[i].point, 12) || !localAddress(actor, output[i].serial, 4) || !localAddress(actor, output[i].visible, 4)
            || !localAddress(actor, output[i].expires, 4) || !localAddress(actor, output[i].scale, 4)) return false;
    }
    activeController = actor; activeWorld = world; regions.reset(); ++counters.bindings;
    capture::setWorld(classifyWorldName(text, length) == WorldKind::Mission ? world : nullptr);
    return true;
}

// Require successful registration as a regular enemy. Unknown/unregistered actors fail closed;
// descriptor budget significance is deliberately not used as a substitute for Pawn counting tags.
bool eligible(const SpawnEvent& event, float& cost) {
    auto* manager = capture::spawnManager();
    if (!manager) return false;
    const auto* regular=static_cast<EnemyBucket*>(value(manager,L"ActiveEnemies"));
    const auto* small=static_cast<EnemyBucket*>(value(manager,L"ActiveSwarmerEnemies"));
    const auto* critters=static_cast<EnemyBucket*>(value(manager,L"ActiveCritters"));
    if (!regular || !small || !critters || !eligibleEnemy(event.pawn,*regular,*small,*critters)) return false;
    wchar_t pawnName[512]{}; const auto length = name(reinterpret_cast<void*>(event.pawn), pawnName, 512);
    if (!length || length >= 512 || wcsstr(pawnName, L"Hoarder") || wcsstr(pawnName, L"Huuli")) return false;
    auto* rating = static_cast<float*>(value(reinterpret_cast<void*>(event.origin.descriptor), L"DifficultyRating"));
    if (!rating || !std::isfinite(*rating) || *rating < 0 || *rating > 1000000.f) return false;
    cost = *rating; // Explicit base descriptor cost; runtime modifier budgeting is not reconstructed here.
    return true;
}
void update(void* actor) {
    if (retired.load(std::memory_order_relaxed) || GetCurrentThreadId() != thread || counters.fault) return;
    if ((activeController != actor || !instanceCookie || *instanceCookie != cookie) && !bind(actor)) { counters.fault = 1; capture::stop(); return; }
    ++counters.frames; capture::poll(counters.frames);
    const auto now = GetTickCount64(); SpawnEvent event;
    const float seconds = *durationSec;
    regions.lifetimeMs = seconds >= 1.0f && seconds <= 30.0f ? static_cast<uint64_t>(seconds * 1000.0f) : OriginRegions::LifetimeMs;
    for (uint32_t i = 0; i < SpawnAttribution::Capacity && capture::pop(event); ++i) {
        float cost = 0;
        if (eligible(event, cost)) {
            if (event.selectedMs && event.capturedMs >= event.selectedMs && event.capturedMs >= event.queuedMs) {
                const auto lead=event.capturedMs-event.selectedMs;
                if (!counters.timingSamples || lead<counters.centerMinMs) counters.centerMinMs=lead;
                ++counters.timingSamples;
                if (lead>counters.centerMaxMs) counters.centerMaxMs=lead;
                if (event.capturedMs-event.queuedMs>counters.queueMaxMs) counters.queueMaxMs=event.capturedMs-event.queuedMs;
                if (now-event.capturedMs>counters.handoffMaxMs) counters.handoffMaxMs=now-event.capturedMs;
            }
            if (*enabledNatural) regions.add(event, now, cost);
        } else ++counters.filtered;
    }
    regions.expire(now);
    if (capture::stats().fault || !*enabledNatural) for (auto& r : regions.items) if (r.visible) { r.visible = 0; ++r.serial; }
    for (uint32_t i = 0; i < OriginRegions::Capacity; ++i) {
        const auto& r = regions.items[i];
        if (*output[i].serial == r.serial) continue;
        memcpy(output[i].point, &r.point.x, 12);
        *output[i].expires = *worldTime + (r.expires > now ? static_cast<float>(r.expires - now) / 1000.f : 0.f);
        *output[i].scale = r.scale();
        *output[i].visible = r.visible; *output[i].serial = r.serial;
    }
    counters.regionsDropped = regions.overflow; counters.stale = regions.stale;
}
void guardedUpdate(void* actor) {
    __try { update(actor); }
    __except(GetExceptionCode() == 0xE06D7363 ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER) {
        counters.fault = 2; capture::stop();
    }
}
// Zero parameters: stock UE4 P_FINISH increments FFrame::Code (audited offset 0x20) unless null.
void nativePoll(void* actor, void* stack, void*) {
    auto*& code = *reinterpret_cast<unsigned char**>(static_cast<unsigned char*>(stack) + 0x20);
    if (code) ++code;
    try { guardedUpdate(actor); } catch (...) { counters.fault = 3; capture::stop(); }
}
capture::Target target(uintptr_t base, uintptr_t rva, const char* hex) {
    capture::Target result; result.address = reinterpret_cast<void*>(base + rva);
    for (size_t i = 0; i < result.bytes.size(); ++i) {
        auto digit = [](char ch) { return ch <= '9' ? ch - '0' : ch - 'a' + 10; };
        result.bytes[i] = static_cast<unsigned char>((digit(hex[2*i]) << 4) | digit(hex[2*i+1]));
    }
    return result;
}
} // namespace
bool configure(HMODULE runtime, HMODULE game, uint32_t gameThread) noexcept {
    thread = gameThread;
    find = resolve<Find>(runtime, "ue4ssl_host_static_find_object_v1", 0x76a600);
    value = resolve<Value>(runtime, "?GetValuePtrByPropertyName@UObject@Unreal@RC@@QEAAPEAXPEB_W@Z", 0x2100a0);
    functionSlot = resolve<GetFunc>(runtime, "?GetFunc@UFunction@Unreal@RC@@QEAAAEAPEAV?$function@$$A6AXPEAVUObject@Unreal@RC@@AEAUFFrame@23@PEAX@Z@std@@XZ", 0x1d6990);
    flags = resolve<GetFlags>(runtime, "?GetFunctionFlags@UFunction@Unreal@RC@@QEAAAEAIXZ", 0x1f8870);
    parameters = resolve<GetSize>(runtime, "?GetParmsSize@UFunction@Unreal@RC@@QEAAAEAGXZ", 0x1f7ee0);
    actorWorld = resolve<GetWorld>(runtime, "ue4ssl_host_actor_get_world_v1", 0x769520);
    name = resolve<GetName>(runtime, "ue4ssl_host_object_full_name_v1", 0x76a090);
    if (!find || !value || !functionSlot || !flags || !parameters || !actorWorld || !name) return false;
    const auto base = reinterpret_cast<uintptr_t>(game);
    capture::Binding binding;
    binding.normal = target(base, 0x19db3a0, "4883ec4833c0488944243048894424380fb6442470884424");
    binding.enqueue = target(base, 0x16571c0, "48895c24184c894c24204889542410555657415441554156");
    binding.actor = target(base, 0x37e89a0, "488bc4555356574154415541564157488da8b8feffff4881");
    binding.shrink = target(base, 0x16517f0, "40534883ec20448b410c488bd948635108458bc8442bca49");
    binding.normalReturn = base + 0x16abe53;
    binding.enqueueReturns[0] = base + 0x19dba4d; binding.enqueueReturns[1] = base + 0x19dbbf6;
    binding.actorReturn = base + 0x166132c; binding.shrinkReturn = base + 0x166161a;
    binding.imageBase = base; binding.imageEnd = base + 0x7000000;
    binding.sourceChain = {base + 0x19dba4d, base + 0x19dbe76, base + 0x19db33d, base + 0x19db3c8, base + 0x16abe53, base + 0x16af697};
    binding.threadId = thread;
    configured = capture::install(binding); return configured;
}
static bool prepareUnchecked() noexcept {
    if (!configured || GetCurrentThreadId() != thread) return false;
    try {
        constexpr wchar_t path[] = L"/Game/NormalWaveIndicator/BP_NwiAuto.BP_NwiAuto_C:NwiPoll";
        const WideView view{path, std::size(path)-1};
        auto* function = find(&view);
        if (!function) return false;
        auto* slot = functionSlot(function); auto* functionFlags = flags(function); auto* size = parameters(function);
        const auto address = reinterpret_cast<uintptr_t>(function);
        auto within = [address](void* p) { const auto v = reinterpret_cast<uintptr_t>(p); return v >= address + 0x30 && v < address + 0x200; };
        if (!within(slot) || !within(functionFlags) || !within(size) || *size != 0) return false;
        // This is our own no-op function only. No global ProcessInternal/ProcessEvent interception is added.
        *slot = &nativePoll; *functionFlags |= 0x400; // FUNC_Native.
        return true;
    } catch (...) { return false; }
}
bool prepareClass(void*) noexcept {
    __try { return prepareUnchecked(); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void stop() noexcept { retired.store(true); capture::stop(); }
Stats stats() noexcept { return counters; }
} // namespace nwi::automatic
