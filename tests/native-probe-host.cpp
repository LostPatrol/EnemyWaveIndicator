// Exercise our DLL in a standalone synthetic host; never load UE4SSL.dll or the game.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include "../mods/EnemyWaveNativeProbe/BuildOptions.h"

using Start = void* (__cdecl*)(const void*);
using Callback = void (__cdecl*)(void*);

// Copy the Windows procedure pointer representation without incompatible-cast warnings.
template<typename T> T resolve(HMODULE module, const char* name) {
    FARPROC address = GetProcAddress(module, name);
    T function = nullptr;
    static_assert(sizeof(function) == sizeof(address));
    memcpy(&function, &address, sizeof(function));
    return function;
}

// Verify loader lifecycle even when another process prevents opening the optional log.
int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 1;
    const std::wstring dllPath = argv[1];
    const auto separator = dllPath.find_last_of(L"\\/");
    if (separator == std::wstring::npos) return 6;
    const std::wstring logPath = dllPath.substr(0, separator + 1) + L"probe-latest.jsonl";
    for (int run = 0; run < 3; ++run) {
        // An exclusive reader reproduces access denied without changing directory permissions.
        HANDLE lockedLog = INVALID_HANDLE_VALUE;
        if (run == 2) {
            lockedLog = CreateFileW(logPath.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (lockedLog == INVALID_HANDLE_VALUE) return 7;
        }
        HMODULE module = LoadLibraryExW(argv[1], nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!module) { printf("LoadLibrary failed: %lu\n", GetLastError()); return 2; }
        auto start = resolve<Start>(module, "ue4ssl_mod_start_v1");
        auto stop = resolve<Callback>(module, "ue4ssl_mod_uninstall_v1");
        auto update = resolve<Callback>(module, "ue4ssl_mod_on_update_v1");
        auto unreal = resolve<Callback>(module, "ue4ssl_mod_on_unreal_init_v1");
        auto ui = resolve<Callback>(module, "ue4ssl_mod_on_ui_init_v1");
        auto program = resolve<Callback>(module, "ue4ssl_mod_on_program_start_v1");
        if (!start || !stop || !update || !unreal || !ui || !program) return 3;
        update(nullptr);
        void* instance = start(nullptr);
        if (!instance || start(nullptr) != instance) return 4;
        program(instance);
        unreal(instance);
        ui(instance);
        // Bogus opaque pointers must be rejected without dereferencing them.
        int unrelated = 0;
        update(&unrelated);
        unreal(&unrelated);
        stop(&unrelated);
        for (int i = 0; i < (run == 2 ? 10 : 10000); ++i) update(instance);
#if EWI_DIAGNOSTICS
        if (run == 0) { Sleep(5100); update(instance); }
#endif
        stop(instance);
        update(instance);
        stop(instance);
        if (!FreeLibrary(module)) return 5;
        if (lockedLog != INVALID_HANDLE_VALUE) CloseHandle(lockedLog);
    }
    puts("PASS: exports, callbacks, invalid handles, reload and startup/updates/uninstall with log access denied (synthetic host).");
    return 0;
}
