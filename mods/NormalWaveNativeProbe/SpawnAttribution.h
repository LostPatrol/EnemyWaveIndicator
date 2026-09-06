// Engine-independent shadow queue. Requires complete, ordered mutation observations on one game thread.
#pragma once
#include <array>
#include <cmath>
#include <cstdint>

namespace nwi {
struct SpawnKey {
    uint64_t descriptor = 0; // Opaque identity only; never dereferenced or owned.
    float x = 0, y = 0, z = 0;
    bool valid() const noexcept {
        return descriptor && std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }
    bool operator==(const SpawnKey& other) const noexcept {
        return descriptor == other.descriptor && x == other.x && y == other.y && z == other.z;
    }
};
struct SpawnEvent {
    uint64_t epoch = 0, wave = 0, pawn = 0, frame = 0;
    SpawnKey origin{};
    uint64_t capturedMs = 0; // Monotonic capture time, for delivery latency and stale-event rejection.
    int64_t capturedQpc = 0; // High-resolution delivery telemetry; never used as a gameplay deadline.
};
enum class AttributionFault : uint32_t { None, Context, Count, Key, Capacity };

// This class does not discover provenance, call Unreal, hook functions, or infer ownership from coordinates.
// The native adapter must observe ALL appends and removals, including non-normal and failed spawns.
class SpawnAttribution {
public:
    static constexpr uint32_t Capacity = 512; // Fixed storage bounds OUR tracking, never the game's queue.
    struct Counters {
        uint64_t appended = 0, removed = 0, published = 0, failed = 0, unclassified = 0, dropped = 0;
    } stats;

    // A monotonic mission epoch defeats reused World/manager addresses. Attach only to a verified empty queue.
    bool begin(uint64_t epoch, uint64_t manager, uint32_t observedCount) noexcept {
        if (!epoch || epoch <= epoch_ || !manager) { stop(AttributionFault::Context); return false; }
        epoch_ = epoch; manager_ = manager; count_ = head_ = size_ = 0;
        fault_ = AttributionFault::None; stats = {};
        if (observedCount) { stop(AttributionFault::Count); return false; }
        return true;
    }
    // Explicit mid-queue attachment: preexisting entries are retained as UNKNOWN, never back-labelled.
    bool attach(uint64_t epoch, uint64_t manager, const SpawnKey* existing, uint32_t count) noexcept {
        if (!begin(epoch, manager, 0)) return false;
        if (count > Capacity || (count && !existing)) { stop(AttributionFault::Capacity); return false; }
        for (uint32_t i = 0; i < count; ++i) {
            if (!existing[i].valid()) { stop(AttributionFault::Key); return false; }
            entries_[count_++] = {existing[i], 0};
        }
        return true;
    }

    // Call after a verified successful append. wave=0 means unknown/non-normal, not a guessed normal wave.
    bool append(uint64_t epoch, uint64_t manager, uint32_t before, uint32_t after,
                const SpawnKey& key, uint64_t wave) noexcept {
        if (!context(epoch, manager)) return false;
        if (before != count_ || after != before + 1) { stop(AttributionFault::Count); return false; }
        if (!key.valid()) { stop(AttributionFault::Key); return false; }
        if (count_ == Capacity) { stop(AttributionFault::Capacity); return false; }
        entries_[count_++] = {key, wave}; ++stats.appended;
        return true;
    }

    // Commit only after observed RemoveAtSwap, using the pre-removal item and actual SpawnActor result.
    // Failure (pawn=0) still consumes ownership. Reallocation needs no update: no queue addresses are stored.
    bool removeSwap(uint64_t epoch, uint64_t manager, uint32_t index, uint32_t before, uint32_t after,
                    const SpawnKey& key, uint64_t pawn, uint64_t frame, uint64_t capturedMs = 0, int64_t capturedQpc = 0) noexcept {
        if (!context(epoch, manager)) return false;
        if (before != count_ || !before || after != before - 1 || index >= count_) {
            stop(AttributionFault::Count); return false;
        }
        if (!(entries_[index].key == key)) { stop(AttributionFault::Key); return false; }
        const auto entry = entries_[index];
        entries_[index] = entries_[--count_]; ++stats.removed;
        if (!pawn) { ++stats.failed; return true; }
        if (!entry.wave) { ++stats.unclassified; return true; }
        if (size_ == Capacity) { ++stats.dropped; return true; } // Drop OUR event; never retry gameplay.
        events_[(head_ + size_) % Capacity] = {epoch_, entry.wave, pawn, frame, key, capturedMs, capturedQpc};
        ++size_; ++stats.published;
        return true;
    }

    // The presentation consumer must also run on the same verified engine thread.
    bool pop(uint64_t epoch, uint64_t currentFrame, uint64_t maxAgeFrames, SpawnEvent& output) noexcept {
        output = {};
        if (fault_ != AttributionFault::None || epoch != epoch_) return false;
        while (size_) {
            const auto event = events_[head_]; head_ = (head_ + 1) % Capacity; --size_;
            if (currentFrame < event.frame || currentFrame - event.frame > maxAgeFrames) {
                ++stats.dropped; continue;
            }
            output = event; return true;
        }
        return false;
    }

    // Every unobserved mutation, adapter read failure or lifecycle loss must stop this epoch immediately.
    void stop(AttributionFault reason = AttributionFault::Context) noexcept {
        if (fault_ == AttributionFault::None) fault_ = reason == AttributionFault::None ? AttributionFault::Context : reason;
        count_ = head_ = size_ = 0;
    }
    AttributionFault fault() const noexcept { return fault_; }
    uint32_t tracked() const noexcept { return count_; }
    uint32_t pending() const noexcept { return size_; }
    bool matches(uint32_t index, const SpawnKey& key) const noexcept {
        return index < count_ && entries_[index].key == key;
    }
private:
    struct Entry { SpawnKey key; uint64_t wave; };
    bool context(uint64_t epoch, uint64_t manager) noexcept {
        if (fault_ != AttributionFault::None) return false;
        if (epoch != epoch_ || manager != manager_) { stop(); return false; }
        return true;
    }
    std::array<Entry, Capacity> entries_{};
    std::array<SpawnEvent, Capacity> events_{};
    uint64_t epoch_ = 0, manager_ = 0;
    uint32_t count_ = 0, head_ = 0, size_ = 0;
    AttributionFault fault_ = AttributionFault::Context;
};
static_assert(sizeof(SpawnAttribution) < 64 * 1024, "Tracking must stay below a fixed 64 KiB budget.");
} // namespace nwi
