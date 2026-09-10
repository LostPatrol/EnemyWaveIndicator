// Bind only our zero-argument Blueprint polling function to a native thunk; never hook global Blueprint execution.
#pragma once
#include <cstdint>
#include <windows.h>
namespace nwi::automatic {
bool configure(HMODULE runtime, HMODULE game, uint32_t gameThread) noexcept;
bool prepareClass(void* cls) noexcept;
void stop() noexcept;
struct Stats {
    uint64_t frames = 0, regionsDropped = 0, stale = 0; uint32_t bindings = 0, fault = 0;
    uint64_t timingSamples = 0, centerMinMs = 0, centerMaxMs = 0, queueMaxMs = 0, handoffMaxMs = 0, filtered = 0;
    uint64_t predictionAttempts = 0, predictionSuccesses = 0, predictionFailures = 0, predictionComparisons = 0;
    uint64_t predictionManagerLookups = 0, predictionManagerResolved = 0, predictionCountdownSamples = 0, predictionWindowSamples = 0;
    uint64_t predictionPlayersReady = 0, predictionGeometryReady = 0, predictionNavigationReady = 0;
    uint64_t predictionProjectionReady = 0, predictionCenterReady = 0;
    uint64_t predictionPlayerLists = 0, predictionControllersResolved = 0;
    uint64_t predictionPawnsResolved = 0, predictionPositionsRead = 0;
    uint64_t predictionNavPointsTotal = 0, predictionNavPointsMax = 0;
    uint64_t predictionCandidatesTotal = 0, predictionCandidatesMax = 0;
    float predictionLastErrorCm = 0, predictionMinErrorCm = 0, predictionMaxErrorCm = 0, predictionErrorTotalCm = 0;
};
Stats stats() noexcept;
}
