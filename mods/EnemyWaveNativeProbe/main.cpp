// Verify engine thread identity, install passive capture and bootstrap the deduplicating native Init actors.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cwchar>
#include <cstring>
#include "DispatchProbe.h"
#include "EngineThreadIdentity.h"
#include "PresentationBootstrap.h"
#include "ModHubRegistration.h"
#include "ActiveWorld.h"
#include "AutomaticPresentation.h"
#include "GameCapture.h"

namespace {
constexpr ULONGLONG kHeartbeatMs = 5000; // Summarize callbacks at most once per five seconds.
HMODULE moduleHandle = nullptr;
SRWLOCK stateLock = SRWLOCK_INIT;
nwi::DispatchProbe dispatchProbe; // Static context is retained by pinning this DLL before enqueueing.
using ThreadPredicate = bool (*)();
ThreadPredicate isInitialized = nullptr;
nwi::EngineThreadIdentity engineIdentity;
nwi::PresentationBootstrap presentation;
nwi::ActiveWorld activeWorld;
using ObjectWorld = void* (*)(void*);
ObjectWorld objectWorld = nullptr;
using ObjectName = size_t (*)(void*, wchar_t*, size_t);
ObjectName objectName = nullptr;
using FindFirst = void* (*)(const wchar_t*);
using SafeProcessEvent = bool (*)(void*, void*, void*);
using GetParmsSize = uint16_t* (*)(void*);
using HasActorOfClass = bool (*)(void*, void*);
decltype(nwi::PresentationApi::find) staticFind = nullptr;
decltype(nwi::PresentationApi::valid) validObject = nullptr;
FindFirst findFirst = nullptr;
SafeProcessEvent safeProcessEvent = nullptr;
GetParmsSize getParmsSize = nullptr;
HasActorOfClass hasActorOfClass = nullptr;
bool apiChecked = false;

// Use named exports only, with the audited image's RVA as an additional compatibility guard.
template<typename T> T resolve(HMODULE module, const char* name, uintptr_t rva) noexcept {
    FARPROC address = GetProcAddress(module, name);
    T result = nullptr;
    static_assert(sizeof(result) == sizeof(address));
    if (reinterpret_cast<uintptr_t>(address) - reinterpret_cast<uintptr_t>(module) == rva)
        memcpy(&result, &address, sizeof(result));
    return result;
}

// The engine owns the authoritative identity. The loader-private flag is retained as diagnostic data.
nwi::ThreadSample sampleThread() noexcept {
    return engineIdentity.sample(GetCurrentThreadId(), isInitialized());
}
uint64_t sampleClock() noexcept { return GetTickCount64(); }

// Keep SEH separate from C++ unwinding; a bridge failure permanently disables this reader.
nwi::WorldResult guardedWorld(void* viewport) {
    __try { return {objectWorld(viewport), false}; }
    __except (GetExceptionCode() == 0xE06D7363 ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER) {
        return {nullptr, true};
    }
}
nwi::WorldResult readViewportWorld(void* viewport) noexcept {
    try { return guardedWorld(viewport); } catch (...) { return {nullptr, true}; }
}
void* currentWorld() noexcept { return activeWorld.read(); }
// Audited host wrapper owns its temporary string and copies into our bounded caller-owned buffer.
nwi::WorldKind worldKind(void* world) noexcept {
    wchar_t name[256]{};
    const auto length = objectName(world, name, std::size(name));
    if (length >= std::size(name)) return nwi::WorldKind::Excluded;
    return nwi::classifyWorldName(name, length);
}

// SearchForMods updates discovery; RefreshPages rebuilds the already-created visible UI.
bool refreshModHubUnchecked(void* world) {
    void* hub = findFirst(L"Mod_ModHub_C");
    if (!validObject(hub) || objectWorld(hub) != world) return false;
    return nwi::rescanAndRefreshModHub({staticFind, validObject, getParmsSize, safeProcessEvent}, hub);
}
bool guardedRefreshModHub(void* world) {
    __try { return refreshModHubUnchecked(world); }
    __except (GetExceptionCode() == 0xE06D7363 ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool refreshModHub(void* world) noexcept {
    try { return guardedRefreshModHub(world); } catch (...) { return false; }
}
bool modHubReadyUnchecked(void* world) {
    constexpr wchar_t path[] = L"/Game/ModHub/Mod_ModHub.Mod_ModHub_C";
    const nwi::WideView name{path, std::size(path) - 1};
    void* cls = staticFind(&name);
    return validObject(cls) && hasActorOfClass(world, cls);
}
bool guardedModHubReady(void* world) {
    __try { return modHubReadyUnchecked(world); }
    __except (GetExceptionCode() == 0xE06D7363 ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool modHubReady(void* world) noexcept {
    try { return guardedModHubReady(world); } catch (...) { return false; }
}

// Only DispatchProbe's per-callback engine identity gate can invoke this action.
void showPresentation(nwi::ThreadSample& sample) noexcept {
    static bool automaticAttempted = false;
    if (!automaticAttempted) {
        automaticAttempted = true;
        if (!nwi::automatic::configure(GetModuleHandleW(L"UE4SSL.dll"), GetModuleHandleW(L"FSD-Win64-Shipping.exe"), sample.tid)) presentation.status = 4;
    }
    presentation.tick();
    if (activeWorld.disabled) presentation.status = 4;
    sample.bootstrapStatus = presentation.status;
    sample.visualActors = presentation.spawned;
    sample.classLoads = presentation.loads;
    sample.bootstrapReadinessChecks = presentation.readinessChecks;
    sample.bootstrapPreparationFailures = presentation.preparationFailures;
    sample.hubRefreshAttempts = presentation.refreshAttempts;
    sample.hubRefreshes = presentation.refreshes;
    sample.hubRefreshFailures = presentation.refreshFailures;
    sample.viewportFinds = activeWorld.finds; sample.worldReads = activeWorld.reads;
    sample.worldChanges = activeWorld.changes; sample.worldFaults = activeWorld.faults;
    sample.activeWorld = activeWorld.lastWorld;
    sample.worldKind = static_cast<uint32_t>(presentation.kind);
    sample.excludedWorlds = presentation.excluded;
    const auto captured = nwi::capture::stats(); const auto automatic = nwi::automatic::stats();
    sample.nativeWaves = captured.waves; sample.tagged = captured.tagged; sample.spawnSuccesses = captured.successes;
    sample.delivered = captured.delivered; sample.deliveryMaxMs = captured.deliveryMaxMs;
    sample.deliveryMaxUs = captured.deliveryMaxUs;
    sample.autoFrames = automatic.frames; sample.regionsDropped = automatic.regionsDropped;
    sample.sourceRejected = captured.sourceRejected; sample.skipped = captured.skipped; sample.failedSpawns = captured.failures;
    sample.lateEvents = automatic.stale; sample.autoBindings = automatic.bindings;
    sample.normalEntries = captured.normalEntries; sample.normalSiteMatches = captured.normalSiteMatches;
    sample.noMissionWorld = captured.noMissionWorld; sample.contextRejected = captured.contextRejected;
    sample.rejectedSourceFrames = captured.rejectedSourceFrames;
    sample.timing = {automatic.timingSamples, automatic.centerMinMs, automatic.centerMaxMs, automatic.queueMaxMs, automatic.handoffMaxMs, automatic.filtered};
    sample.captureFault = captured.fault; sample.autoFault = automatic.fault; sample.hookStatus = captured.hookStatus;
}

// Called after Unreal initialization, never from DllMain. No cancellation/unload barrier is assumed.
void configureDispatch() noexcept {
    if (apiChecked) return;
    apiChecked = true;
    const HMODULE runtime = GetModuleHandleW(L"UE4SSL.dll");
    if (!runtime) return; // Standalone lifecycle tests intentionally have no runtime.
    auto dispatch = resolve<decltype(nwi::ProbeApi::dispatch)>(runtime,
        "ue4ssl_host_dispatch_on_game_thread_v1", 0x769670);
    auto initialized = resolve<ThreadPredicate>(runtime, "?IsGameThreadInitialized@Unreal@RC@@YA_NXZ", 0x232a80);
    engineIdentity.configure(GetModuleHandleW(L"FSD-Win64-Shipping.exe"));
    if (!dispatch || !initialized || !engineIdentity.available()) return;
    HMODULE retained = nullptr;
    constexpr DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN;
    if (!GetModuleHandleExW(flags, reinterpret_cast<LPCWSTR>(moduleHandle), &retained)
        || !GetModuleHandleExW(flags, reinterpret_cast<LPCWSTR>(runtime), &retained)) return;
    isInitialized = initialized;
    auto find = resolve<decltype(nwi::PresentationApi::find)>(runtime, "ue4ssl_host_static_find_object_v1", 0x76a600);
    auto load = resolve<decltype(nwi::PresentationApi::loadClass)>(runtime, "ue4ssl_host_load_class_asset_blocking_v1", 0x769cc0);
    auto valid = resolve<decltype(nwi::PresentationApi::valid)>(runtime, "ue4ssl_host_is_valid_object_v1", 0x769cb0);
    auto spawn = resolve<decltype(nwi::PresentationApi::spawn)>(runtime, "ue4ssl_host_spawn_actor_v1", 0x76a330);
    auto findViewport = resolve<decltype(nwi::WorldApi::findViewport)>(runtime,
        "?SafeFindFirstOf@Seh@RC@@YAPEAVUObject@Unreal@2@PEB_W@Z", 0x78efa0);
    objectWorld = resolve<ObjectWorld>(runtime, "?GetWorld@UObject@Unreal@RC@@QEBAPEAVUWorld@23@XZ", 0x20cbd0);
    objectName = resolve<ObjectName>(runtime, "ue4ssl_host_object_full_name_v1", 0x76a090);
    auto process = resolve<SafeProcessEvent>(runtime,
        "?SafeProcessEvent@Seh@RC@@YA_NPEAVUObject@Unreal@2@PEAVUFunction@42@PEAX@Z", 0x78efd0);
    auto parms = resolve<GetParmsSize>(runtime, "?GetParmsSize@UFunction@Unreal@RC@@QEAAAEAGXZ", 0x1f7ee0);
    auto hasActor = resolve<HasActorOfClass>(runtime, "ue4ssl_host_has_actor_of_class_v1", 0x769a80);
    if (find && load && valid && spawn && findViewport && objectWorld && objectName && process && parms && hasActor) {
        staticFind = find; validObject = valid; findFirst = findViewport;
        safeProcessEvent = process; getParmsSize = parms; hasActorOfClass = hasActor;
        activeWorld.configure({findViewport, valid, &readViewportWorld});
        presentation.configure({find, load, valid, spawn, &currentWorld, &worldKind,
            &modHubReady, &nwi::automatic::prepareClass, &refreshModHub});
    }
    dispatchProbe.configure({dispatch, &sampleThread, &sampleClock, &showPresentation});
}
struct State {
    HANDLE log = INVALID_HANDLE_VALUE;
    ULONGLONG started = 0, lastReport = 0, updates = 0, threadChanges = 0;
    LONGLONG previousQpc = 0, minGap = 0, maxGap = 0, gapTotal = 0, frequency = 0;
    DWORD previousThread = 0;
    bool active = false;
} state;

// The caller holds stateLock. Closing on write failure avoids repeated I/O errors.
void record(const char* event) noexcept {
    if (state.log == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME utc{};
    GetSystemTime(&utc);
    const double scale = state.frequency ? 1000.0 / static_cast<double>(state.frequency) : 0.0;
    const auto& probe = dispatchProbe.stats;
    char line[4096];
    const int size = sprintf_s(line,
        "{\"probe\":\"0.9.1\",\"event\":\"%s\",\"utc\":\"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\","
        "\"pid\":%lu,\"tid\":%lu,\"elapsed_ms\":%llu,\"updates\":%llu,\"thread_changes\":%llu,"
        "\"gap_min_ms\":%.6f,\"gap_max_ms\":%.6f,\"gap_mean_ms\":%.6f,"
        "\"dispatch_enabled\":%s,\"dispatch_pending\":%s,\"dispatch_disabled\":%s,"
        "\"dispatch_queued\":%llu,\"dispatch_completed\":%llu,\"dispatch_rejected\":%llu,"
        "\"dispatch_timeout\":%llu,\"dispatch_stale\":%llu,\"dispatch_game_thread\":%llu,"
        "\"dispatch_other_thread\":%llu,\"dispatch_uninitialized\":%llu,\"dispatch_tid\":%u,"
        "\"dispatch_latency_max_ms\":%llu,\"dispatch_latency_mean_ms\":%.3f,"
        "\"identity_source\":\"engine_globals\",\"engine_identity_available\":%s,\"engine_thread_id\":%u,"
        "\"runtime_initialized_samples\":%llu,\"identity_read_failures\":%llu,"
        "\"bootstrap_status\":%u,\"visual_test_actors\":%u,\"visual_class_loads\":%u,"
        "\"bootstrap_readiness_checks\":%u,\"bootstrap_preparation_failures\":%u,"
        "\"hub_refresh_attempts\":%u,\"hub_refreshes\":%u,\"hub_refresh_failures\":%u,"
        "\"world_source\":\"game_viewport\",\"viewport_finds\":%u,\"world_reads\":%u,"
        "\"world_changes\":%u,\"world_faults\":%u,\"last_valid_world\":%llu,"
        "\"world_kind\":%u,\"excluded_worlds\":%u,"
        "\"native_waves\":%llu,\"tagged_spawns\":%llu,\"spawn_successes\":%llu,\"normal_events_delivered\":%llu,"
        "\"auto_frames\":%llu,\"delivery_max_ms\":%llu,\"delivery_max_us\":%llu,\"regions_dropped\":%llu,"
        "\"source_rejected\":%llu,\"enqueue_without_append\":%llu,\"failed_spawns\":%llu,\"late_events\":%llu,\"auto_bindings\":%u,"
        "\"normal_entries\":%llu,\"normal_site_matches\":%llu,\"normal_no_mission_world\":%llu,\"normal_context_rejected\":%llu,"
        "\"rejected_source_rvas\":[%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu],"
        "\"timing\":[%llu,%llu,%llu,%llu,%llu,%llu],"
        "\"capture_fault\":%u,\"auto_fault\":%u,\"hook_status\":%u}\n",
        event, utc.wYear, utc.wMonth, utc.wDay, utc.wHour, utc.wMinute, utc.wSecond, utc.wMilliseconds,
        GetCurrentProcessId(), GetCurrentThreadId(), GetTickCount64() - state.started,
        state.updates, state.threadChanges, state.minGap * scale, state.maxGap * scale,
        state.updates > 1 ? state.gapTotal * scale / static_cast<double>(state.updates - 1) : 0.0,
        dispatchProbe.enabled() ? "true" : "false", dispatchProbe.pending() ? "true" : "false",
        probe.disabled ? "true" : "false", probe.queued, probe.completed, probe.rejected,
        probe.timedOut, probe.stale, probe.gameThread, probe.otherThread, probe.uninitialized, probe.lastTid,
        probe.latencyMax, probe.completed ? static_cast<double>(probe.latencyTotal) / probe.completed : 0.0,
        engineIdentity.available() ? "true" : "false", probe.expectedTid, probe.runtimeInitialized, probe.identityReadFailures,
        probe.bootstrapStatus, probe.visualActors, probe.classLoads,
        probe.bootstrapReadinessChecks, probe.bootstrapPreparationFailures,
        probe.hubRefreshAttempts, probe.hubRefreshes, probe.hubRefreshFailures,
        probe.viewportFinds, probe.worldReads, probe.worldChanges, probe.worldFaults, probe.activeWorld,
        probe.worldKind, probe.excludedWorlds, probe.nativeWaves, probe.tagged, probe.spawnSuccesses, probe.delivered,
        probe.autoFrames, probe.deliveryMaxMs, probe.deliveryMaxUs, probe.regionsDropped,
        probe.sourceRejected, probe.skipped, probe.failedSpawns, probe.lateEvents, probe.autoBindings,
        probe.normalEntries, probe.normalSiteMatches, probe.noMissionWorld, probe.contextRejected,
        probe.rejectedSourceFrames[0], probe.rejectedSourceFrames[1], probe.rejectedSourceFrames[2], probe.rejectedSourceFrames[3],
        probe.rejectedSourceFrames[4], probe.rejectedSourceFrames[5], probe.rejectedSourceFrames[6], probe.rejectedSourceFrames[7],
        probe.timing[0],probe.timing[1],probe.timing[2],probe.timing[3],probe.timing[4],probe.timing[5],
        probe.captureFault, probe.autoFault, probe.hookStatus);
    DWORD written = 0;
    if (size <= 0 || !WriteFile(state.log, line, static_cast<DWORD>(size), &written, nullptr)
        || written != static_cast<DWORD>(size)) {
        CloseHandle(state.log);
        state.log = INVALID_HANDLE_VALUE;
    }
}

// Store each run beside our own DLL. CREATE_NEW preserves all earlier test evidence.
HANDLE openLog() noexcept {
    wchar_t directory[32768];
    constexpr DWORD capacity = sizeof(directory) / sizeof(directory[0]);
    const DWORD length = GetModuleFileNameW(moduleHandle, directory, capacity);
    if (!length || length >= capacity) return INVALID_HANDLE_VALUE;
    wchar_t* slash = wcsrchr(directory, L'\\');
    if (!slash) return INVALID_HANDLE_VALUE;
    slash[1] = L'\0';
    wchar_t path[32768];
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
        if (swprintf_s(path, L"%sprobe-%lu-%llu-%u.jsonl", directory,
            GetCurrentProcessId(), GetTickCount64(), attempt) < 0) return INVALID_HANDLE_VALUE;
        HANDLE log = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (log != INVALID_HANDLE_VALUE || GetLastError() != ERROR_FILE_EXISTS) return log;
    }
    return INVALID_HANDLE_VALUE;
}

void lifecycle(void* instance, const char* event) noexcept {
    if (instance != &state) return;
    AcquireSRWLockExclusive(&stateLock);
    if (state.active) record(event);
    ReleaseSRWLockExclusive(&stateLock);
}
} // namespace

// Loader-lock work is limited to remembering the module path source; no I/O or engine calls.
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) moduleHandle = instance;
    return TRUE;
}

// Source contract: cpp_mod.rs at UE4SS.Lite commit 2a47524; opaque context is never dereferenced.
extern "C" __declspec(dllexport) void* ue4ssl_mod_start_v1(const void*) noexcept {
    AcquireSRWLockExclusive(&stateLock);
    if (!state.active) {
        state = State{};
        state.log = openLog();
        if (state.log == INVALID_HANDLE_VALUE) {
            ReleaseSRWLockExclusive(&stateLock);
            return nullptr;
        }
        state.active = true;
        dispatchProbe.start();
        state.started = state.lastReport = GetTickCount64();
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);
        state.frequency = frequency.QuadPart;
        record("start");
    }
    ReleaseSRWLockExclusive(&stateLock);
    return &state;
}

extern "C" __declspec(dllexport) void ue4ssl_mod_on_program_start_v1(void* instance) noexcept {
    lifecycle(instance, "program_start");
}
extern "C" __declspec(dllexport) void ue4ssl_mod_on_unreal_init_v1(void* instance) noexcept {
    if (instance != &state) return;
    AcquireSRWLockExclusive(&stateLock);
    if (state.active) { configureDispatch(); record("unreal_init"); }
    ReleaseSRWLockExclusive(&stateLock);
}
extern "C" __declspec(dllexport) void ue4ssl_mod_on_ui_init_v1(void* instance) noexcept {
    lifecycle(instance, "ui_init");
}

// These are loader update intervals, not game frame times; do not infer the game thread from them.
extern "C" __declspec(dllexport) void ue4ssl_mod_on_update_v1(void* instance) noexcept {
    if (instance != &state) return;
    AcquireSRWLockExclusive(&stateLock);
    if (state.active) {
        // The callback never takes stateLock; the optional bootstrap runs only after engine thread verification.
        dispatchProbe.pump();
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        const DWORD thread = GetCurrentThreadId();
        if (state.updates > 0) {
            const LONGLONG gap = now.QuadPart - state.previousQpc;
            if (state.updates == 1 || gap < state.minGap) state.minGap = gap;
            if (gap > state.maxGap) state.maxGap = gap;
            state.gapTotal += gap;
            if (thread != state.previousThread) ++state.threadChanges;
        }
        state.previousQpc = now.QuadPart;
        state.previousThread = thread;
        ++state.updates;
        const ULONGLONG tick = GetTickCount64();
        if (state.updates == 1 || tick - state.lastReport >= kHeartbeatMs) {
            record(state.updates == 1 ? "first_update" : "heartbeat");
            state.lastReport = tick;
        }
    }
    ReleaseSRWLockExclusive(&stateLock);
}

// Only loader-driven uninstall is recorded. Forced process termination may omit this event.
extern "C" __declspec(dllexport) void ue4ssl_mod_uninstall_v1(void* instance) noexcept {
    if (instance != &state) return;
    AcquireSRWLockExclusive(&stateLock);
    if (state.active) {
        dispatchProbe.stop();
        nwi::automatic::stop();
        record("uninstall");
        state.active = false;
        if (state.log != INVALID_HANDLE_VALUE) CloseHandle(state.log);
        state.log = INVALID_HANDLE_VALUE;
    }
    ReleaseSRWLockExclusive(&stateLock);
}


