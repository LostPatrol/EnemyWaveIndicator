<!-- Native observation module overview; internal source directory retains its historical name. -->
# Normal Wave Indicator — native component 0.6.0

This directory contains the native DLL source. It installs as `ue4ss/mods/NormalWaveIndicator/main.dll` and requires the matching presentation Pak. See the repository README for exact supported game/runtime hashes and source-build instructions.

`GameCapture.cpp` observes four audited native sites, forwarding the original calls exactly once. `SpawnAttribution.h` tracks fixed-capacity queue provenance. `OriginRegions.h` groups successful origins within 8 m and applies configurable expiry. `AutomaticPresentation.cpp` writes only our own Blueprint properties on the verified game thread; pooled widgets and sphere components perform rendering.

`DispatchProbe.h` bootstraps and summarizes at low frequency. A callback on another thread performs no UObject work, retains the previous gameplay snapshot and can be followed by a later valid callback. Callback contexts and code remain alive for the process lifetime.

These APIs and layouts are version-specific. Do not weaken the compatibility checks or substitute estimated locations for successfully attributed events. Never reintroduce the retired JavaScript enemy-spawn hooks. Logs remain file-only and are not public release contents.