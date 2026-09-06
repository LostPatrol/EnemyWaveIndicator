<!-- Native alpha packaging scope and unmet subscription-only release acceptance criteria. -->
# Early test release preparation

Product: **Normal Wave Indicator**. Version: **0.6.0 alpha**.

This build requires two original components: a native observation DLL loaded by the specifically supported UE4SSL runtime, and a cooked presentation Pak. **Subscribing to a Pak on mod.io alone cannot install the native component.** A mod.io-only installation route and its moderation acceptance have not been verified. Do not advertise one-click mod.io support or a Verified classification.

**2026-09-07 decision: NO-GO for the requested subscription-only release.** See [the technical audit](MODIO-FEASIBILITY.md). This is a runtime compatibility blocker, not just an uncompleted upload checklist. Keep this version as a developer native alpha. Do not upload its ZIP as a working native-mod.io release.

`Prepare-Release.ps1 -Target NativeAlpha` checks offline pass flags and recorded source/asset/cook/config hashes, then creates an explicitly non-release-ready native archive. `-Target Modio` stops before writing any output. It cannot be enabled by changing a manifest boolean: implement and test a genuine Pak-only event source and initialization path first.

The release archive must contain only our native DLL, our Pak, an installation script, documentation, SHA-256 manifest and the MinHook license. Do not bundle the game executable, UE4SSL, Unreal Engine, Mod Hub, DRGlib, original mini-MULE assets, extracted assets, private logs or editor helpers.

## Feature description for the native alpha (not an upload-ready listing)

Normal Wave Indicator shows where a normal wave's enemies actually spawn. Pulsing spheres mark nearby spawn regions; warning text follows the world position each frame and shows straight-line distance in meters. Offscreen origins retain an edge direction indicator.

Open the installed Mod Hub with H, select Normal Wave Indicator, and customize the warning text, persistence, sphere size and RGB intensity. Text pulsing can be disabled. Settings are local and saved independently of the game save.

Host-local alpha: install on the host to display indicators for that host. Client display and network synchronization are not implemented. Normal-wave attribution observes the audited game's native queue source and successful enemy creation; unrelated event waves are excluded. Nearby origins within eight meters are grouped, with a maximum of eight simultaneous markers. A frequent three-marker result is not a hardcoded three-marker limit.

## Before publishing a binary release or mod.io listing

- Complete real H-menu discovery, apply/save/restart, in-view/offscreen distance and flash tests on the new build.
- Test several natural waves, event waves and mini-MULE transitions; include swarmer eggs when available.
- Check long missions, map changes and multiplayer host behavior. Current bootstrap supports up to eight observed world identities and approximately 46 minutes of startup/transition monitoring per process; already running frame rendering continues afterward.
- Measure performance under heavy load. Headless and synthetic tests do not establish GPU cost or first-visible-frame timing.
- Confirm current DRG mod.io distribution rules and category with the moderators. Native-loader installation remains an explicit external dependency.
- Add real screenshots, dependency links, exact supported game/runtime hashes, version notes and rollback instructions. Do not mark unchecked items as passed.

References: [DRG Blueprint Modding Handbook](https://drg-modding.github.io/docs/guides/blueprint-modding-guide.html), [official modding FAQ](https://www.deeprockgalactic.com/modding-support-faq), [mod.io approval checklist](https://mod.io/g/drg/r/approval-process-and-checklist-for-upload).
