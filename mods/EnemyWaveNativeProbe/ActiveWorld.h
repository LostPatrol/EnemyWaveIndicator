// Obtain the current World from the live game viewport; never infer activity from a loaded map asset.
#pragma once
#include <cstdint>
namespace nwi {
struct WorldResult { void* world = nullptr; bool fault = false; };
struct WorldApi {
    void* (*findViewport)(const wchar_t*) = nullptr;
    bool (*valid)(void*) = nullptr;
    WorldResult (*worldOf)(void*) = nullptr;
};
class ActiveWorld {
public:
    uint32_t finds = 0, reads = 0, changes = 0, faults = 0;
    bool disabled = false;
    uintptr_t lastWorld = 0; // Historical identity token; a null travel read does not make this current.
    void configure(WorldApi api) noexcept { api_ = api; }
    void* read() noexcept {
        if (disabled || !api_.findViewport) return nullptr;
        if (!api_.valid(viewport_)) {
            if (finds >= 8) { disabled = true; return nullptr; } // No persistent registry scan loop.
            ++finds;
            viewport_ = api_.findViewport(L"GameViewportClient");
            if (!api_.valid(viewport_)) return nullptr;
        }
        ++reads;
        const auto result = api_.worldOf(viewport_);
        if (result.fault) { ++faults; disabled = true; return nullptr; }
        if (!api_.valid(result.world)) return nullptr; // Travel may temporarily have no World.
        if (lastWorld != reinterpret_cast<uintptr_t>(result.world)) {
            lastWorld = reinterpret_cast<uintptr_t>(result.world); ++changes;
        }
        return result.world;
    }
private:
    WorldApi api_{};
    void* viewport_ = nullptr; // Revalidate before each call; this object normally survives map travel.
};
}
