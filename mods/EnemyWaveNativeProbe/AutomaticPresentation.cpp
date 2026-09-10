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
#include "NaturalWavePrediction.h"

#ifndef NWI_NATURAL_PREDICTION
#define NWI_NATURAL_PREDICTION 0
#endif

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
using ProcessEvent = bool (*)(void*, void*, void*);
Find find = nullptr; Value value = nullptr; GetFunc functionSlot = nullptr;
GetFlags flags = nullptr; GetSize parameters = nullptr; GetWorld actorWorld = nullptr; GetName name = nullptr;
ProcessEvent processEvent = nullptr;
uint32_t thread = 0; bool configured = false; Stats counters; OriginRegions regions;
void* activeController = nullptr; void* activeWorld = nullptr;
std::atomic<bool> retired{false};
int32_t* instanceCookie = nullptr;
float* durationSec = nullptr; // Own Blueprint property; never a game-class field.
float* worldTime = nullptr;
int32_t* enabledTypes[WaveTypeCount]{};
int32_t cookie = 0; // A fresh Blueprint instance has zero even when its allocator reuses an old address.
struct Output { float* point = nullptr; int32_t* serial = nullptr; int32_t* visible = nullptr; float* expires = nullptr; float* scale = nullptr; int32_t* type = nullptr; };
Output output[OriginRegions::Capacity]{};

#if NWI_NATURAL_PREDICTION
using NavQuery = bool (*)(void*, uint8_t, uint8_t, const prediction::Position*, float, prediction::Position*);
NavQuery projectToNav = nullptr, findSpawnCenter = nullptr;
prediction::CountdownGate predictionGate;
prediction::Position lastPrediction{};
uint64_t lastPredictionMs = 0, comparedWave = 0;
void* predictionGameMode = nullptr; void* predictionManager = nullptr;
void* waveManagerFunction = nullptr;

struct ObjectArray { void** data = nullptr; int32_t count = 0, capacity = 0; };

bool bindNavQuery(uintptr_t base, uintptr_t rva, const unsigned char (&expected)[24], NavQuery& outputFunction) noexcept {
    auto* raw = reinterpret_cast<unsigned char*>(base + rva);
    if (std::memcmp(raw, expected, sizeof(expected))) return false;
    std::memcpy(&outputFunction, &raw, sizeof(outputFunction));
    return true;
}

void* reflectedObject(void* object, const wchar_t* field) {
    auto* slot = static_cast<void**>(value(object, field));
    return slot ? *slot : nullptr;
}

void* resolveWaveManager(void* gameMode) {
    if (!gameMode || !waveManagerFunction) return nullptr;
    ++counters.predictionManagerLookups;
    struct Parameters { void* result = nullptr; } callParameters{};
    if (!processEvent(gameMode, waveManagerFunction, &callParameters) || !callParameters.result) return nullptr;
    ++counters.predictionManagerResolved;
    return callParameters.result;
}

void hidePrediction() noexcept {
    for (auto& region : regions.items) if (waveType(region.wave) == prediction::RegionType && region.visible) {
        region.visible = 0; ++region.serial;
    }
}

void showPrediction(const prediction::Position& point, uint64_t now) noexcept {
    OriginRegions::Region* selected = nullptr;
    for (auto& region : regions.items) if (waveType(region.wave) == prediction::RegionType) { selected = &region; break; }
    if (!selected) for (auto& region : regions.items) if (!region.visible) { selected = &region; break; }
    if (!selected) { ++regions.overflow; return; }
    selected->point = {1, point.x, point.y, point.z};
    selected->wave = waveIdentity(counters.predictionAttempts, prediction::RegionType);
    selected->expires = now + 5500; selected->weight = 0; selected->count = 0;
    selected->visible = 1; ++selected->serial;
}

bool readPlayers(void* gameMode, prediction::Position (&players)[4], uint32_t& count) {
    count = 0;
    const auto* controllers = static_cast<ObjectArray*>(value(gameMode, L"PlayerControllers"));
    if (!controllers || controllers->count < 1 || controllers->count > 4 || controllers->capacity < controllers->count || !controllers->data) return false;
    for (int32_t i = 0; i < controllers->count; ++i) {
        void* controller = controllers->data[i];
        void* pawn = reflectedObject(controller, L"Pawn");
        if (!pawn) pawn = reflectedObject(controller, L"AcknowledgedPawn");
        void* root = pawn ? reflectedObject(pawn, L"RootComponent") : nullptr;
        if (!root) continue;
        // The audited natural selector reads the scene-component translation at +0x1d0 in this game image.
        const auto point = *reinterpret_cast<prediction::Position*>(static_cast<unsigned char*>(root) + 0x1d0);
        if (prediction::finite(point)) players[count++] = point;
    }
    return count != 0;
}

bool calculatePrediction(void* gameMode, prediction::Position& result) {
    prediction::Position players[4]{}; uint32_t count = 0;
    if (!readPlayers(gameMode, players, count)) return false;
    ++counters.predictionPlayersReady;
    const auto geometry = prediction::playerSphere(players, count);
    if (!geometry.valid) return false;
    ++counters.predictionGeometryReady;
    // The selector follows UActorComponent::WorldPrivate -> UWorld+0x120 -> navigation owner+0x420.
    // Test.2 incorrectly skipped the intermediate UWorld+0x120 object and therefore never reached Pathfinder.
    void* pathfinder = prediction::resolvePathfinder(activeWorld);
    if (!pathfinder || !*reinterpret_cast<unsigned char*>(static_cast<unsigned char*>(pathfinder) + 0x18)) return false;
    ++counters.predictionNavigationReady;
    prediction::Position projected{};
    if (!projectToNav(pathfinder, 1, 2, &geometry.center, geometry.radius + prediction::ProjectionPaddingCm, &projected)) return false;
    ++counters.predictionProjectionReady;
    if (!findSpawnCenter(pathfinder, 0, 2, &projected, geometry.radius + prediction::SpawnDistanceCm, &result)
        || !prediction::finite(result)) return false;
    ++counters.predictionCenterReady;
    return true;
}

void samplePrediction(uint64_t now) {
    // GameMode and wave-manager lifetimes match this bound mission controller; retry only while startup is incomplete.
    if ((!predictionGameMode || !predictionManager) && counters.frames % 60 == 0) {
        predictionGameMode = reflectedObject(activeWorld, L"AuthorityGameMode");
        predictionManager = resolveWaveManager(predictionGameMode);
    }
    if (!predictionManager) return;
    const auto* bytes = static_cast<unsigned char*>(predictionManager);
    const float seconds = *reinterpret_cast<const float*>(bytes + 0x138);
    const bool enabled = bytes[0x111] != 0;
    const bool blocked = *reinterpret_cast<const int32_t*>(bytes + 0x148) != 0;
    ++counters.predictionCountdownSamples;
    if (std::isfinite(seconds) && seconds > 0 && seconds <= prediction::LeadSeconds) ++counters.predictionWindowSamples;
    if (std::isfinite(seconds) && seconds > prediction::LeadSeconds + 0.25f) hidePrediction();
    if (!predictionGate.sample(seconds, enabled, blocked)) return;
    ++counters.predictionAttempts;
    prediction::Position point{};
    if (!calculatePrediction(predictionGameMode, point)) { ++counters.predictionFailures; hidePrediction(); return; }
    ++counters.predictionSuccesses; lastPrediction = point; lastPredictionMs = now;
    showPrediction(point, now);
}

void comparePrediction(const SpawnEvent& event, uint64_t now) noexcept {
    if (waveType(event.wave) != 0 || event.wave == comparedWave || !event.center.valid()
        || !lastPredictionMs || now < lastPredictionMs || now - lastPredictionMs > 15000) return;
    comparedWave = event.wave;
    const float x = event.center.x - lastPrediction.x, y = event.center.y - lastPrediction.y, z = event.center.z - lastPrediction.z;
    const float error = std::sqrt(x*x + y*y + z*z);
    if (!std::isfinite(error)) return;
    counters.predictionLastErrorCm = error; counters.predictionErrorTotalCm += error;
    if (!counters.predictionComparisons || error < counters.predictionMinErrorCm) counters.predictionMinErrorCm = error;
    if (error > counters.predictionMaxErrorCm) counters.predictionMaxErrorCm = error;
    ++counters.predictionComparisons; hidePrediction();
}
#endif

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
    if (!localAddress(actor, abi, 4) || *abi != 0x90400) return false;
    worldTime = static_cast<float*>(value(actor, L"NativeTime"));
    for (uint32_t i = 0; i < WaveTypeCount; ++i) {
        wchar_t field[48]; if (i) swprintf_s(field, L"NativeEnabled%u", i); else wcscpy_s(field, L"NativeEnabled");
        enabledTypes[i] = static_cast<int32_t*>(value(actor, field));
        if (!localAddress(actor, enabledTypes[i], 4)) return false;
    }
    if (!localAddress(actor, worldTime, 4)) return false;
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
        swprintf_s(field, L"RegionType%u", i); output[i].type = static_cast<int32_t*>(value(actor, field));
        if (!localAddress(actor, output[i].point, 12) || !localAddress(actor, output[i].serial, 4) || !localAddress(actor, output[i].visible, 4)
            || !localAddress(actor, output[i].expires, 4) || !localAddress(actor, output[i].scale, 4) || !localAddress(actor, output[i].type, 4)) return false;
    }
    activeController = actor; activeWorld = world; regions.reset(); ++counters.bindings;
#if NWI_NATURAL_PREDICTION
    predictionGate.reset(); lastPredictionMs = comparedWave = 0;
    predictionGameMode = reflectedObject(world, L"AuthorityGameMode");
    predictionManager = resolveWaveManager(predictionGameMode);
#endif
    capture::setWorld(classifyWorldName(text, length) == WorldKind::Mission ? world : nullptr);
    return true;
}

// Natural waves exclude small enemies; explicitly enabled scripted swarmers remain displayable.
// Unknown/unregistered actors and critters fail closed for both paths.
bool eligible(const SpawnEvent& event, float& cost) {
    auto* manager = capture::spawnManager();
    if (!manager) return false;
    const auto* regular=static_cast<EnemyBucket*>(value(manager,L"ActiveEnemies"));
    const auto* small=static_cast<EnemyBucket*>(value(manager,L"ActiveSwarmerEnemies"));
    const auto* critters=static_cast<EnemyBucket*>(value(manager,L"ActiveCritters"));
    if (!regular || !small || !critters) return false;
    const bool accepted = waveType(event.wave) == 0 ? eligibleEnemy(event.pawn,*regular,*small,*critters)
        : eligibleScriptedEnemy(event.pawn,*regular,*small,*critters);
    if (!accepted) return false;
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
#if NWI_NATURAL_PREDICTION
    samplePrediction(now);
#endif
    const float seconds = *durationSec;
    regions.lifetimeMs = seconds >= 1.0f && seconds <= 30.0f ? static_cast<uint64_t>(seconds * 1000.0f) : OriginRegions::LifetimeMs;
    for (uint32_t i = 0; i < SpawnAttribution::Capacity && capture::pop(event); ++i) {
#if NWI_NATURAL_PREDICTION
        comparePrediction(event, now);
#endif
        float cost = 0;
        if (eligible(event, cost)) {
            if (waveType(event.wave) == 0 && event.selectedMs && event.capturedMs >= event.selectedMs && event.capturedMs >= event.queuedMs) {
                const auto lead=event.capturedMs-event.selectedMs;
                if (!counters.timingSamples || lead<counters.centerMinMs) counters.centerMinMs=lead;
                ++counters.timingSamples;
                if (lead>counters.centerMaxMs) counters.centerMaxMs=lead;
                if (event.capturedMs-event.queuedMs>counters.queueMaxMs) counters.queueMaxMs=event.capturedMs-event.queuedMs;
                if (now-event.capturedMs>counters.handoffMaxMs) counters.handoffMaxMs=now-event.capturedMs;
            }
            const auto type = waveType(event.wave);
            if (type < WaveTypeCount && *enabledTypes[type]) regions.add(event, now, cost);
        } else ++counters.filtered;
    }
    regions.expire(now);
    for (auto& r : regions.items) if (r.visible && (capture::stats().fault
        || (waveType(r.wave) != prediction::RegionType && (waveType(r.wave) >= WaveTypeCount || !*enabledTypes[waveType(r.wave)])))) { r.visible = 0; ++r.serial; }
    for (uint32_t i = 0; i < OriginRegions::Capacity; ++i) {
        const auto& r = regions.items[i];
        if (*output[i].serial == r.serial) continue;
        memcpy(output[i].point, &r.point.x, 12);
        *output[i].expires = *worldTime + (r.expires > now ? static_cast<float>(r.expires - now) / 1000.f : 0.f);
        *output[i].scale = waveType(r.wave) == prediction::RegionType ? 0.6f : r.scale();
        *output[i].type = static_cast<int32_t>(waveType(r.wave));
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
// Match the initiating controller/event's exact class path, never an enemy name or an active-wave list.
// Stock scripted callers pass EX_Self as WorldContextObject (recorded in the source audits).
int32_t sourceClassUnchecked(void* context, void* world) {
    if (!context || actorWorld(context) != world) return -1;
    auto* cls = *reinterpret_cast<void**>(static_cast<unsigned char*>(context) + 0x10);
    wchar_t text[512]{}; const auto length = name(cls, text, std::size(text));
    if (!length || length >= std::size(text)) return -1;
    const auto* path = wcschr(text, L' '); if (!path) return -1;
    for (uint32_t i = 1; i < WaveTypeCount; ++i) if (!wcscmp(path + 1, WaveTypes[i].classPath)) return static_cast<int32_t>(i);
    for (const auto& alias : WaveSourceAliases) if (alias.type < WaveTypeCount && !wcscmp(path + 1, alias.classPath)) return static_cast<int32_t>(alias.type);
    return -1;
}
int32_t sourceClass(void* context, void* world) noexcept {
    __try { return sourceClassUnchecked(context, world); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return -1; }
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
    processEvent = resolve<ProcessEvent>(runtime, "?SafeProcessEvent@Seh@RC@@YA_NPEAVUObject@Unreal@2@PEAVUFunction@42@PEAX@Z", 0x78efd0);
    if (!find || !value || !functionSlot || !flags || !parameters || !actorWorld || !name || !processEvent) return false;
    const auto base = reinterpret_cast<uintptr_t>(game);
#if NWI_NATURAL_PREDICTION
    constexpr unsigned char projectBytes[24]{0x80,0x79,0x18,0x00,0x4d,0x8b,0xd1,0x0f,0x84,0xf9,0x00,0x00,0x00,0x0f,0xb6,0xc2,0x45,0x0f,0xb6,0xc0,0x41,0x8d,0x50,0xff};
    constexpr unsigned char centerBytes[24]{0x48,0x83,0xec,0x48,0x48,0x8b,0x44,0x24,0x78,0xf3,0x0f,0x10,0x44,0x24,0x70,0x48,0x89,0x44,0x24,0x30,0xf3,0x0f,0x11,0x44};
    if (!bindNavQuery(base, 0x4019320, projectBytes, projectToNav)
        || !bindNavQuery(base, 0x40079e0, centerBytes, findSpawnCenter)) return false;
    constexpr wchar_t managerPath[] = L"/Script/FSD.FSDGameMode:GetWaveManager";
    const WideView managerView{managerPath, std::size(managerPath)-1};
    waveManagerFunction = find(&managerView);
    if (!waveManagerFunction || *parameters(waveManagerFunction) != sizeof(void*)) return false;
#endif
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
    binding.pool = target(base, 0x19db030, "4c894424185355574881eca00000000fb605a2f1ab04498b");
    binding.location = target(base, 0x19dab90, "4055565741554156488d6c24f04881ec10010000488b058d");
    binding.group = target(base, 0x19dbf00, "4c8bdc49895b1849896b20565741564881eca0000000803d");
    binding.spread = target(base, 0x19dc1b0, "4c8bdc53565741564881ec98000000803d22e0ab0405488d");
    binding.spreadCallback = target(base, 0x19dc470, "405355565741574881eca0000000803d63ddab0405498bf1");
    binding.center = target(base, 0x19db3d0, "40555356574154415541564157488dac2428ffffff4881ec");
    binding.classifySource = &sourceClass;
    configured = capture::install(binding); return configured;
}
static bool prepareUnchecked() noexcept {
    if (!configured || GetCurrentThreadId() != thread) return false;
    try {
        constexpr wchar_t path[] = L"/Game/EnemyWaveIndicator/BP_NwiAuto.BP_NwiAuto_C:NwiPoll";
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
