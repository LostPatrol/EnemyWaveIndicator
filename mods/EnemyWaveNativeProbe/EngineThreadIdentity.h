// Read only UE's thread-id scalars from this audited game image; never write memory or call game code.
#pragma once
#include <windows.h>
#include <cstring>
#include "DispatchProbe.h"
#include "GameBindings.h"

namespace ewi {
class EngineThreadIdentity {
public:
    static constexpr size_t SnapshotSize = 17; // uint32 thread ID at +0; initialized byte at +16.
    // Decode the same scalar layout after discovery proves the startup stores and named Windows import.
    static ThreadSample decode(const unsigned char* bytes, size_t count, DWORD tid, bool runtimeReady) noexcept {
        ThreadSample result{tid, false, false};
        result.runtimeInitialized = runtimeReady;
        result.identityReadOk = bytes && count == SnapshotSize;
        if (!result.identityReadOk) return result;
        memcpy(&result.expectedTid, bytes, sizeof(result.expectedTid));
        // Unknown flag representations are rejected, not coerced into a successful thread check.
        result.initialized = bytes[16] == 1 && result.expectedTid != 0;
        result.gameThread = result.initialized && result.expectedTid == tid;
        return result;
    }
    void configure(HMODULE game) noexcept {
        address_ = 0;
        if (!game) return;
        const auto& found = bindings::discover(game).resolution;
        if (found.ok) address_ = reinterpret_cast<uintptr_t>(game) + found.threadIdRva;
    }
    bool available() const noexcept { return address_ != 0; }
    ThreadSample sample(DWORD tid, bool runtimeReady) const noexcept {
        unsigned char bytes[SnapshotSize]{};
        SIZE_T count = 0;
        // RPM fails cleanly on an unreadable range; one bounded read, no UObject dereference.
        if (!address_ || !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address_),
            bytes, sizeof(bytes), &count)) return decode(nullptr, 0, tid, runtimeReady);
        return decode(bytes, count, tid, runtimeReady);
    }
private:
    uintptr_t address_ = 0;
};
} // namespace ewi
