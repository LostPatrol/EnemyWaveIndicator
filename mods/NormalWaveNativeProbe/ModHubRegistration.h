// Invoke Mod Hub's discovery and visible-page rebuild as one zero-parameter registration transaction.
#pragma once
#include <cstddef>
#include <cstdint>
#include <iterator>
#include "PresentationBootstrap.h"

namespace nwi {
struct ModHubRegistrationApi {
    void* (*find)(const WideView*) = nullptr;
    bool (*valid)(void*) = nullptr;
    uint16_t* (*parameterSize)(void*) = nullptr;
    bool (*process)(void*, void*, void*) = nullptr;
};
inline constexpr wchar_t ModHubSearchPath[] = L"/Game/ModHub/Mod_ModHub.Mod_ModHub_C:SearchForMods";
inline constexpr wchar_t ModHubRefreshPath[] = L"/Game/ModHub/Mod_ModHub.Mod_ModHub_C:RefreshPages";

template<size_t N> bool callModHubNoArgs(ModHubRegistrationApi api, void* hub, const wchar_t (&path)[N]) {
    const WideView name{path, N - 1};
    void* function = api.find(&name);
    if (!api.valid(function)) return false;
    uint16_t* size = api.parameterSize(function);
    return size && *size == 0 && api.process(hub, function, nullptr);
}
inline bool rescanAndRefreshModHub(ModHubRegistrationApi api, void* hub) {
    if (!api.find || !api.valid || !api.parameterSize || !api.process || !hub) return false;
    return callModHubNoArgs(api, hub, ModHubSearchPath)
        && callModHubNoArgs(api, hub, ModHubRefreshPath);
}
} // namespace nwi
