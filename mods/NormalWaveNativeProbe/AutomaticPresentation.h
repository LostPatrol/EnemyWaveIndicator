// Bind only our zero-argument Blueprint polling function to a native thunk; never hook global Blueprint execution.
#pragma once
#include <cstdint>
#include <windows.h>
namespace nwi::automatic {
bool configure(HMODULE runtime, HMODULE game, uint32_t gameThread) noexcept;
bool prepareClass(void* cls) noexcept;
void stop() noexcept;
struct Stats { uint64_t frames = 0, regionsDropped = 0, stale = 0; uint32_t bindings = 0, fault = 0; };
Stats stats() noexcept;
}
