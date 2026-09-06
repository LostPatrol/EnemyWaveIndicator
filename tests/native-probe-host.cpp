// Exercise our DLL in a standalone synthetic host; never load UE4SSL.dll or the game.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>

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

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 1;
    for (int run = 0; run < 2; ++run) {
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
        for (int i = 0; i < 10000; ++i) update(instance);
        if (run == 0) { Sleep(5100); update(instance); }
        stop(instance);
        update(instance);
        stop(instance);
        if (!FreeLibrary(module)) return 5;
    }
    puts("PASS: exports, start, callbacks, invalid handles, 20001 updates, uninstall and reload (synthetic host).");
    return 0;
}
