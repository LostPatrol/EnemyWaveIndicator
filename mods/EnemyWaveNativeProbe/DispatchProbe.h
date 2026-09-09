// Single-slot dispatch gate. Owner methods are serialized; complete() may run concurrently.
#pragma once
#include <atomic>
#include <cstdint>
#include <array>

namespace nwi {
struct ThreadSample {
    uint32_t tid = 0;
    bool initialized = false, gameThread = false;
    uint32_t expectedTid = 0;
    bool runtimeInitialized = false, identityReadOk = true;
    bool actionRan = false; // Invalid-thread samples must never erase the last gameplay snapshot.
    uint32_t bootstrapStatus = 0, visualActors = 0, classLoads = 0;
    uint32_t bootstrapReadinessChecks = 0, bootstrapPreparationFailures = 0;
    uint32_t hubRefreshAttempts = 0, hubRefreshes = 0, hubRefreshFailures = 0;
    uint32_t viewportFinds = 0, worldReads = 0, worldChanges = 0, worldFaults = 0;
    uint64_t activeWorld = 0;
    uint32_t worldKind = 0, excludedWorlds = 0;
    uint64_t nativeWaves = 0, tagged = 0, spawnSuccesses = 0, delivered = 0, autoFrames = 0, deliveryMaxMs = 0, deliveryMaxUs = 0, regionsDropped = 0;
    uint32_t captureFault = 0, autoFault = 0, hookStatus = 0, autoBindings = 0;
    uint64_t sourceRejected = 0, skipped = 0, failedSpawns = 0, lateEvents = 0;
    uint64_t normalEntries = 0, normalSiteMatches = 0, noMissionWorld = 0, contextRejected = 0;
    std::array<uint64_t, 8> rejectedSourceFrames{};
    std::array<uint64_t, 6> timing{}; // count, center minimum/maximum, queue maximum, handoff maximum, excluded.
};
using Callback = void (*)(void*);
struct ProbeApi {
    bool (*dispatch)(Callback, void*) = nullptr;
    ThreadSample (*sample)() = nullptr;
    uint64_t (*clock)() = nullptr;
    void (*gameThreadAction)(ThreadSample&) = nullptr;
};

// Production instances and their callback code must live until process termination.
class DispatchProbe {
public:
    static constexpr uint64_t IntervalMs = 1000; // Never turn this diagnostic into a frame loop.
    static constexpr uint64_t ReadinessIntervalMs = 250; // Temporary condition polling; no minimum startup delay.
    static constexpr uint64_t SlowIntervalMs = 5000; // Permanent low-cost map-lifecycle cadence after startup.
    static constexpr uint64_t FastRequests = 60;
    static constexpr uint64_t TimeoutMs = 15000;
    struct Stats {
        uint64_t queued = 0, completed = 0, rejected = 0, timedOut = 0, stale = 0;
        uint64_t gameThread = 0, otherThread = 0, uninitialized = 0, latencyTotal = 0, latencyMax = 0;
        uint64_t runtimeInitialized = 0, identityReadFailures = 0;
        uint32_t lastTid = 0, expectedTid = 0;
        uint32_t bootstrapStatus = 0, visualActors = 0, classLoads = 0;
        uint32_t bootstrapReadinessChecks = 0, bootstrapPreparationFailures = 0;
        uint32_t hubRefreshAttempts = 0, hubRefreshes = 0, hubRefreshFailures = 0;
        uint32_t viewportFinds = 0, worldReads = 0, worldChanges = 0, worldFaults = 0;
        uint64_t activeWorld = 0;
        uint32_t worldKind = 0, excludedWorlds = 0;
        uint64_t nativeWaves = 0, tagged = 0, spawnSuccesses = 0, delivered = 0, autoFrames = 0, deliveryMaxMs = 0, deliveryMaxUs = 0, regionsDropped = 0;
        uint32_t captureFault = 0, autoFault = 0, hookStatus = 0, autoBindings = 0;
        uint64_t sourceRejected = 0, skipped = 0, failedSpawns = 0, lateEvents = 0;
        uint64_t normalEntries = 0, normalSiteMatches = 0, noMissionWorld = 0, contextRejected = 0;
        std::array<uint64_t, 8> rejectedSourceFrames{};
    std::array<uint64_t, 6> timing{}; // count, center minimum/maximum, queue maximum, handoff maximum, excluded.
        bool disabled = false;
    } stats;

    // Configure once, before the first request; API pointers stay immutable thereafter.
    void configure(ProbeApi api) noexcept { api_ = api; }
    bool enabled() const noexcept { return api_.dispatch != nullptr; }
    bool pending() const noexcept { return slot_.load(std::memory_order_acquire) == 1; }
    void start() noexcept {
        stats = Stats{};
        nextAt_ = 0;
        const auto generation = generation_.load() + 1;
        generation_.store(generation);
        active_.store(true);
        // Do not reset slot_: an old callback may already have left the host queue.
    }
    void stop() noexcept { active_.store(false); }

    // Called by the loader update thread. Probe logging remains outside the callback.
    void pump() noexcept {
        if (!enabled() || !active_.load()) return;
        const uint64_t now = api_.clock();
        const int slot = slot_.load(std::memory_order_acquire);
        if (slot == 2) {
            if (requestGeneration_ == generation_.load()) {
                ++stats.completed;
                stats.lastTid = result_.tid;
                stats.expectedTid = result_.expectedTid;
                if (result_.actionRan) {
                stats.bootstrapStatus = result_.bootstrapStatus;
                stats.visualActors = result_.visualActors;
                stats.classLoads = result_.classLoads;
                stats.bootstrapReadinessChecks = result_.bootstrapReadinessChecks;
                stats.bootstrapPreparationFailures = result_.bootstrapPreparationFailures;
                stats.hubRefreshAttempts = result_.hubRefreshAttempts;
                stats.hubRefreshes = result_.hubRefreshes;
                stats.hubRefreshFailures = result_.hubRefreshFailures;
                stats.viewportFinds = result_.viewportFinds; stats.worldReads = result_.worldReads;
                stats.worldChanges = result_.worldChanges; stats.worldFaults = result_.worldFaults;
                stats.activeWorld = result_.activeWorld;
                stats.worldKind = result_.worldKind; stats.excludedWorlds = result_.excludedWorlds;
                stats.nativeWaves = result_.nativeWaves; stats.tagged = result_.tagged;
                stats.spawnSuccesses = result_.spawnSuccesses; stats.delivered = result_.delivered;
                stats.autoFrames = result_.autoFrames; stats.deliveryMaxMs = result_.deliveryMaxMs;
                stats.deliveryMaxUs = result_.deliveryMaxUs; stats.regionsDropped = result_.regionsDropped; stats.captureFault = result_.captureFault;
                stats.autoFault = result_.autoFault; stats.hookStatus = result_.hookStatus;
                stats.sourceRejected = result_.sourceRejected; stats.skipped = result_.skipped;
                stats.failedSpawns = result_.failedSpawns; stats.lateEvents = result_.lateEvents; stats.autoBindings = result_.autoBindings;
                stats.normalEntries = result_.normalEntries; stats.normalSiteMatches = result_.normalSiteMatches;
                stats.noMissionWorld = result_.noMissionWorld; stats.contextRejected = result_.contextRejected;
                stats.rejectedSourceFrames = result_.rejectedSourceFrames; stats.timing = result_.timing;
                }
                if (result_.runtimeInitialized) ++stats.runtimeInitialized;
                if (!result_.identityReadOk) { ++stats.identityReadFailures; stats.disabled = true; }
                if (!result_.initialized) ++stats.uninitialized;
                else if (result_.gameThread) ++stats.gameThread;
                else { ++stats.otherThread; } // Reject this callback only; retry on the normal bounded schedule.
                const auto latency = finishedAt_ - queuedAt_;
                stats.latencyTotal += latency;
                if (latency > stats.latencyMax) stats.latencyMax = latency;
            } else ++stats.stale;
            slot_.store(0, std::memory_order_release);
        } else if (slot == 1) {
            if (now - queuedAt_ >= TimeoutMs && !stats.disabled) {
                ++stats.timedOut;
                stats.disabled = true;
            }
            return; // Never replace a context that the host might still call.
        }
        const bool waitingForReadiness = bootstrapPending();
        // This callback also owns map-lifecycle recovery, so the five-second cadence must outlive
        // the original diagnostic window and continue until uninstall or a verified hard fault.
        if (stats.disabled || now < nextAt_) return;
        queuedAt_ = now;
        requestGeneration_ = generation_.load();
        // While startup conditions are pending, sample promptly; after 60 requests fall back to 1 Hz.
        // This is a polling cadence, not a mandatory delay: a ready first sample initializes immediately.
        const auto interval = waitingForReadiness
            ? (stats.queued + 1 < FastRequests ? ReadinessIntervalMs : IntervalMs)
            : (stats.queued + 1 < FastRequests ? IntervalMs : SlowIntervalMs);
        nextAt_ = now + interval;
        slot_.store(1, std::memory_order_release);
        ++stats.queued;
        if (!api_.dispatch(&complete, this)) {
            // A false return is rejection in the audited ABI. Fail closed even if unexpected.
            ++stats.rejected;
            stats.disabled = true;
            // Preserve the slot forever unless a callback actually completes: no cancel barrier.
        }
    }
private:
    bool bootstrapPending() const noexcept {
        if (!api_.gameThreadAction) return false;
        const auto value = stats.bootstrapStatus;
        return value == 0 || value == 1 || value == 2 || value == 6 || value == 7;
    }
    static void complete(void* context) noexcept {
        auto& self = *static_cast<DispatchProbe*>(context);
        ThreadSample result{};
        if (self.active_.load() && self.requestGeneration_ == self.generation_.load())
            result = self.api_.sample();
        if (result.identityReadOk && result.initialized && result.gameThread && self.api_.gameThreadAction
            && self.api_.clock() - self.queuedAt_ < TimeoutMs
            && self.active_.load() && self.requestGeneration_ == self.generation_.load())
        {
            self.api_.gameThreadAction(result);
            result.actionRan = true;
        }
        self.result_ = result;
        self.finishedAt_ = self.api_.clock();
        // This release must be our last access: the owner may reuse the slot afterwards.
        self.slot_.store(2, std::memory_order_release);
    }
    ProbeApi api_{};
    std::atomic<bool> active_{false};
    std::atomic<uint64_t> generation_{0};
    std::atomic<int> slot_{0}; // 0 = idle, 1 = host owns request, 2 = callback published result.
    uint64_t nextAt_ = 0, queuedAt_ = 0, finishedAt_ = 0, requestGeneration_ = 0;
    ThreadSample result_{};
};
} // namespace nwi
