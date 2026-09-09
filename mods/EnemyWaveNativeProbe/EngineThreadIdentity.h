// Read only UE's thread-id scalars from this audited game image; never write memory or call game code.
#pragma once
#include <windows.h>
#include <cstring>
#include "DispatchProbe.h"

namespace nwi {
class EngineThreadIdentity {
public:
    static constexpr uintptr_t InitCodeRva = 0x864655;
    static constexpr uintptr_t ThreadIdRva = 0x65170b8;
    static constexpr size_t SnapshotSize = 17; // uint32 thread ID at +0; initialized byte at +16.
    // FEngineLoop::PreInit stores Windows GetCurrentThreadId and the initialized flag here.
    inline static constexpr unsigned char Signature[] = {
        0xff,0x15,0x9d,0xb1,0xdf,0x03,0x49,0x8b,0xcf,0x44,0x88,0x35,
        0x63,0x2a,0xcb,0x05,0x89,0x05,0x4d,0x2a,0xcb,0x05};
    static bool matches(const unsigned char* bytes, size_t count) noexcept {
        return bytes && count == sizeof(Signature) && memcmp(bytes, Signature, count) == 0;
    }
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
        if (!game) return;
        unsigned char bytes[sizeof(Signature)]{};
        SIZE_T count = 0;
        const uintptr_t base = reinterpret_cast<uintptr_t>(game);
        if (ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(base + InitCodeRva),
            bytes, sizeof(bytes), &count) && matches(bytes, count)) address_ = base + ThreadIdRva;
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
} // namespace nwi
