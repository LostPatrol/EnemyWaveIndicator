// Cache one read-only structural discovery per game process; no game code is invoked by discovery.
#pragma once
#include <windows.h>
#include <mutex>
#include "GameBindingProfile.generated.h"

namespace ewi::bindings {
struct ProductionState {
    Resolution resolution{}; uint32_t imageSize = 0;
    uint64_t elapsedMs = 0; bool attempted = false;
};

// Share diagnostics across the loader and presentation translation units.
inline ProductionState& state() noexcept {
    static ProductionState value;
    return value;
}

// Read PE headers through RPM before granting direct access to the loaded game's mapped sections.
inline uint32_t moduleSize(HMODULE module) noexcept {
    IMAGE_DOS_HEADER dos{}; IMAGE_NT_HEADERS64 nt{}; SIZE_T copied = 0;
    if (!module || !ReadProcessMemory(GetCurrentProcess(), module, &dos, sizeof(dos), &copied)
        || copied != sizeof(dos) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0) return 0;
    const auto at = reinterpret_cast<uintptr_t>(module) + static_cast<uint32_t>(dos.e_lfanew);
    if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(at), &nt, sizeof(nt), &copied)
        || copied != sizeof(nt) || nt.Signature != IMAGE_NT_SIGNATURE
        || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return 0;
    return nt.OptionalHeader.SizeOfImage;
}

// Resolve every required site and relationship together, once; unrelated modules are never scanned.
inline const ProductionState& discover(HMODULE game) noexcept {
    if (!game) return state();
    static std::once_flag once;
    try {
        std::call_once(once, [game] {
            auto& value = state(); value.attempted = true;
            const auto started = GetTickCount64();
            value.imageSize = moduleSize(game);
            Image image;
            if (!value.imageSize || !openImage(game, value.imageSize, image))
                value.resolution.error = "Cannot read mapped game PE";
            else resolve(image, profile::Data, value.resolution);
            value.elapsedMs = GetTickCount64() - started;
        });
    } catch (...) {
        state().resolution.ok = false;
        state().resolution.error = "Binding discovery failed";
    }
    return state();
}
} // namespace ewi::bindings
