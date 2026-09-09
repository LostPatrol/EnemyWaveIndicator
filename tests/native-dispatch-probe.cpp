// Synthetic scheduling tests for the production single-slot core; no UE4SSL or game is loaded.
#include "../mods/NormalWaveNativeProbe/DispatchProbe.h"
#include <cstdio>
#include <thread>
#include "../mods/NormalWaveNativeProbe/EngineThreadIdentity.h"

namespace {
uint64_t now = 0;
nwi::Callback queuedCallback = nullptr;
void* queuedContext = nullptr;
bool synchronous = false, reject = false;
nwi::ThreadSample sample{42, true, true};
unsigned calls = 0;
unsigned actions = 0;
uint32_t actionStatus = 3;
void action(nwi::ThreadSample& value) {
    ++actions; value.bootstrapStatus = actionStatus; value.visualActors = 1;
    value.bootstrapReadinessChecks = 4; value.bootstrapPreparationFailures = 0;
    value.hubRefreshAttempts = 2; value.hubRefreshes = 1; value.hubRefreshFailures = 1;
}
bool enqueue(nwi::Callback callback, void* context) {
    ++calls;
    if (reject) return false;
    if (synchronous) callback(context);
    else { queuedCallback = callback; queuedContext = context; }
    return true;
}
uint64_t clockNow() { return now; }
nwi::ThreadSample readThread() { return sample; }
void drain() { auto cb = queuedCallback; auto ctx = queuedContext; queuedCallback = nullptr; cb(ctx); }
void reset(nwi::DispatchProbe& p) {
    now = 0; calls = 0; synchronous = reject = false; actionStatus = 3;
    queuedCallback = nullptr; sample = {42, true, true};
    p.configure({enqueue, readThread, clockNow}); p.start();
}
#define REQUIRE(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
}
int main() {
    {
        // An off-thread delivery executes no gameplay action and retains the previous snapshot.
        nwi::DispatchProbe p; reset(p); actions = 0;
        p.configure({enqueue, readThread, clockNow, action}); p.pump(); drain(); p.pump();
        REQUIRE(actions == 1 && p.stats.visualActors == 1 && p.stats.bootstrapReadinessChecks == 4
            && p.stats.hubRefreshes == 1 && p.stats.hubRefreshFailures == 1);
        sample.gameThread = false; now = 1000; p.pump(); drain(); p.pump();
        REQUIRE(actions == 1 && p.stats.otherThread == 1 && p.stats.visualActors == 1 && !p.stats.disabled);
        sample.gameThread = true; now = 2000; p.pump(); drain(); p.pump();
        REQUIRE(actions == 2 && p.stats.gameThread == 2);
    }
    {
        // Pending readiness receives prompt condition checks, but a ready first sample has no forced delay.
        nwi::DispatchProbe p; reset(p); actions = 0; actionStatus = 2;
        p.configure({enqueue, readThread, clockNow, action}); p.pump(); drain(); p.pump();
        REQUIRE(calls == 1 && p.stats.bootstrapStatus == 2);
        now = 249; p.pump(); REQUIRE(calls == 1);
        now = 250; p.pump(); REQUIRE(calls == 2); drain(); actionStatus = 3; p.pump();
        REQUIRE(p.stats.bootstrapStatus == 2); // The queued callback sampled before the state change.
        now = 500; p.pump(); drain(); p.pump();
        REQUIRE(p.stats.bootstrapStatus == 3);
    }
    {
        // Waiting for a real World never expires at the post-readiness diagnostic request cap.
        nwi::DispatchProbe p; reset(p); synchronous = true; actionStatus = 2;
        p.configure({enqueue, readThread, clockNow, action});
        for (unsigned i = 0; i < 601; ++i) { now = i * 1000ULL; p.pump(); }
        REQUIRE(calls == 601 && p.stats.bootstrapStatus == 2);
        actionStatus = 3; now = 601000; p.pump(); p.pump();
        REQUIRE(calls == 602 && p.stats.bootstrapStatus == 3);
    }
    // Object actions require a current, timely, initialized game-thread sample.
    for (int mode = 0; mode < 7; ++mode) {
        nwi::DispatchProbe p; reset(p); actions = 0;
        p.configure({enqueue, readThread, clockNow, action});
        if (mode == 1) sample.identityReadOk = false;
        if (mode == 2) sample.initialized = false;
        if (mode == 3) sample.gameThread = false;
        p.pump();
        if (mode == 4) p.stop();
        if (mode == 5) { p.stop(); p.start(); }
        if (mode == 6) { now = 15000; p.pump(); }
        drain(); p.pump();
        REQUIRE(actions == (mode == 0 ? 1u : 0u));
        if (mode == 0) REQUIRE(p.stats.bootstrapStatus == 3 && p.stats.visualActors == 1);
        if (queuedCallback) { p.stop(); drain(); }
    }
    {
        // Decode actual engine scalar layout independently from the loader-private initialization flag.
        unsigned char bytes[nwi::EngineThreadIdentity::SnapshotSize]{};
        bytes[0] = 42; bytes[16] = 1;
        auto result = nwi::EngineThreadIdentity::decode(bytes, sizeof(bytes), 42, false);
        REQUIRE(result.initialized && result.gameThread && result.expectedTid == 42 && !result.runtimeInitialized);
        REQUIRE(!nwi::EngineThreadIdentity::decode(bytes, sizeof(bytes), 43, true).gameThread);
        bytes[16] = 0; REQUIRE(!nwi::EngineThreadIdentity::decode(bytes, sizeof(bytes), 42, true).initialized);
        bytes[16] = 2; REQUIRE(!nwi::EngineThreadIdentity::decode(bytes, sizeof(bytes), 42, true).initialized);
        bytes[0] = 0; bytes[16] = 1; REQUIRE(!nwi::EngineThreadIdentity::decode(bytes, sizeof(bytes), 0, false).initialized);
        REQUIRE(!nwi::EngineThreadIdentity::decode(bytes, sizeof(bytes) - 1, 42, false).identityReadOk);
        REQUIRE(!nwi::EngineThreadIdentity::decode(nullptr, 0, 42, false).identityReadOk);
        unsigned char signature[sizeof(nwi::EngineThreadIdentity::Signature)];
        memcpy(signature, nwi::EngineThreadIdentity::Signature, sizeof(signature));
        REQUIRE(nwi::EngineThreadIdentity::matches(signature, sizeof(signature)));
        for (size_t i = 0; i < sizeof(signature); ++i) {
            signature[i] ^= 1; REQUIRE(!nwi::EngineThreadIdentity::matches(signature, sizeof(signature))); signature[i] ^= 1;
        }
        nwi::EngineThreadIdentity identity; identity.configure(nullptr);
        REQUIRE(!identity.available() && !identity.sample(42, false).identityReadOk);
    }
    {
        nwi::DispatchProbe p; reset(p); p.pump();
        for (now = 1; now < 1000; ++now) p.pump();
        REQUIRE(calls == 1 && p.pending());
        now = 1200; drain(); p.pump();
        REQUIRE(p.stats.completed == 1 && p.stats.gameThread == 1 && p.stats.latencyMax == 1200);
        REQUIRE(calls == 2); drain(); p.pump();
    }
    {
        nwi::DispatchProbe p; reset(p); synchronous = true;
        for (unsigned i = 0; i < 60; ++i) { now = i * 1000ULL; p.pump(); }
        now = 63999; p.pump(); REQUIRE(calls == 60);
        now = 64000; p.pump(); REQUIRE(calls == 61);
        p.pump(); REQUIRE(p.stats.completed == 61);
    }
    {
        nwi::DispatchProbe p; reset(p); synchronous = true;
        for (unsigned i = 0; i < 2000; ++i) { now = i * 5000ULL; p.pump(); }
        REQUIRE(calls == 600 && p.stats.completed == 600 && !p.pending());
    }
    {
        nwi::DispatchProbe p; reset(p); sample.identityReadOk = false; p.pump(); drain(); p.pump();
        REQUIRE(p.stats.identityReadFailures == 1 && p.stats.disabled && calls == 1);
    }
    {
        nwi::DispatchProbe p; reset(p); p.pump();
        now = 15000; p.pump();
        REQUIRE(p.stats.timedOut == 1 && p.stats.disabled && calls == 1);
        now = 90000; p.pump(); REQUIRE(calls == 1);
        drain(); p.pump(); REQUIRE(p.stats.completed == 1 && calls == 1);
    }
    {
        nwi::DispatchProbe p; reset(p); p.pump();
        // Simulate a callback detached from the host queue before uninstall/restart.
        p.stop(); p.start(); now = 500; p.pump(); REQUIRE(calls == 1);
        drain(); p.pump(); REQUIRE(p.stats.stale == 1 && p.stats.completed == 0 && calls == 2);
        drain(); p.pump(); REQUIRE(p.stats.completed == 1);
    }
    {
        nwi::DispatchProbe p; reset(p); p.pump(); p.stop(); drain();
        p.pump(); REQUIRE(p.stats.completed == 0);
        p.start(); p.pump(); REQUIRE(p.stats.stale == 1); drain(); p.pump();
    }
    {
        nwi::DispatchProbe p; reset(p); reject = true; p.pump();
        now = 20000; p.pump(); REQUIRE(calls == 1 && p.stats.rejected == 1 && p.stats.disabled);
    }
    {
        nwi::DispatchProbe p; reset(p); sample.gameThread = false; p.pump(); drain(); p.pump();
        REQUIRE(p.stats.otherThread == 1 && !p.stats.disabled && calls == 1);
        now = 1000; sample.gameThread = true; p.pump(); drain(); p.pump();
        REQUIRE(p.stats.gameThread == 1 && calls == 2);
    }
    {
        nwi::DispatchProbe p; reset(p); sample.initialized = false; p.pump(); drain(); p.pump();
        REQUIRE(p.stats.uninitialized == 1 && p.stats.gameThread == 0 && !p.stats.disabled);
    }
    {
        nwi::DispatchProbe p; reset(p);
        // Exercise release/acquire publication on a separate OS thread, including stop in flight.
        for (unsigned i = 0; i < 300; ++i) {
            now = i * 5000ULL; p.pump();
            std::thread worker(drain);
            if (i == 299) p.stop();
            worker.join(); p.pump();
        }
        REQUIRE(p.stats.completed == 299 && calls == 300);
        p.start(); p.pump(); REQUIRE(p.stats.stale == 1); drain(); p.pump();
    }
    puts("PASS: engine scalar layout, 22-byte signature mutations, failed reads, queue bound, throttle, 600 cap, timeout, late callback, restart, rejection, thread mismatch, initialization, concurrent completion.");
    return 0;
}
