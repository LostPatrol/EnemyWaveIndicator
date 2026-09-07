// Bind only our zero-argument Blueprint polling function to a native thunk; never hook global Blueprint execution.
#pragma once
#include <cstdint>
#include <windows.h>
namespace nwi::automatic {
bool configure(HMODULE runtime, HMODULE game, uint32_t gameThread) noexcept;
bool prepareClass(void* cls) noexcept;
void stop() noexcept;
struct Stats { uint64_t frames = 0, regionsDropped = 0, stale = 0; uint32_t bindings = 0, fault = 0; uint64_t timingSamples = 0, centerMinMs = 0, centerMaxMs = 0, queueMaxMs = 0, handoffMaxMs = 0, filtered = 0; };
Stats stats() noexcept;
}
