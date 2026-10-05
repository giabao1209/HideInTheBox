# Validation record

Date: 2026-10-04. Source handoff from an environment without Unreal Engine.

## Executed

- `python Scripts/validate_source.py`: PASS, 16 headers; project/targets/config, local project includes, generated-header order, hygiene and documentation links.
- `ASAN_OPTIONS=detect_leaks=0 python Scripts/test_rules.py`: PASS, 3413 checks against the actual portable runtime gameplay and performance headers, compiled using g++ C++17 with warnings as errors and AddressSanitizer/UndefinedBehaviorSanitizer. Coverage: sampled radial dead-zone inputs, wave bounds, mana/cooldown transaction rejection, shared ability cooldown, greed claim idempotency and integer overflow. Most checks are sampled stick inputs, not separate gameplay scenarios.
- LeakSanitizer disabled because this sandbox cannot inspect processes through /proc; leak detection is not claimed. Address/undefined-behavior sanitizers remained enabled.
- `git diff --check`: PASS.
- Source review: timer/delegate/binding cleanup, weak target lifetime, snapshot enemy iteration during damage, one hit per swing, explicit montage failure, command cancellation, remapping validation, transient effects cleanup. Inspection is not runtime evidence.

## Not executed

- UnrealHeaderTool / UnrealBuildTool: NOT RUN.
- Unreal automation `HideInTheBox.Rules`: NOT RUN.
- PIE / Standalone / animation/material wiring: NOT RUN.
- Physical gamepad and focus/disconnect/reconnect: NOT RUN.
- Windows cook/package: NOT RUN. Create/save DemoArena and set startup maps first.
- Console: no platform SDK or hardware validation.

Static/native-rule checks cannot establish that reflected code compiles or that the game works in Unreal. Keep the PR draft until the Editor build and runtime acceptance checklist have actual results. Follow EDITOR_SETUP_VI.md; record failures with the first compiler/runtime error, engine version and tested commit.

## Debug-system update

Static validation and diff whitespace checks rerun after the debug changes: PASS. Review covered bounded history/message length, world-local ownership without Actor references, filter/freeze/clear semantics, timer cleanup, paused/Game Over page switching, and Shipping guards. The native-rule suite still covers gameplay rules only; it does not execute UHBDebugSubsystem. Debug runtime acceptance cases are in DEBUG_SYSTEM_VI.md and remain NOT RUN without Unreal.

## FPS update

- Added 21 native assertions for actual HBFrameStats code: empty/warm-up, reciprocal FPS vs count/time average, duplicate-frame rejection, pause/resume, skipped draws, real 500ms hitch retention, invalid/backward clock, reset, capacity240, eviction totals and chronological graph order.
- Executed `ASAN_OPTIONS=detect_leaks=0 python Scripts/test_rules.py`: PASS, 3413 total gameplay/performance checks with ASan/UBSan. LeakSanitizer remains disabled for the sandbox restriction above.
- Executed static validation (16 headers) and `git diff --check`: PASS.
- HUD integration, pause behavior in engine, FPS comparison, graph rendering/resolution and Shipping build: NOT RUN. Native tests do not measure the game's actual FPS.

## Map-authored level and Blueprint asset handoff — 2026-10-05

- Replaced runtime primitive arena construction with actor discovery by documented Actor Tags. DemoArena now owns its geometry, lighting, objectives, cache meshes, spawn anchors and build sockets. Enemy and turret classes are selected from the Blueprint GameMode defaults; objective/cache actors are placed in the map.
- Added Blueprint-assignable phase/wave/enemy events for optional presentation. The C++ GameMode still owns phase progression, resource changes, enemy spawning rules, wave timing, construction and repairs.
- Editor guide now walks through DemoArena layout, actor tags, BP_DemoPlayer camera/mesh setup, structure/enemy child Blueprints, AnimBP locomotion → Slot → Output Pose, and attack montage notify.
- `python Scripts/validate_source.py`: PASS (16 headers and project/doc consistency checks).
- `ASAN_OPTIONS=detect_leaks=0 python Scripts/test_rules.py`: PASS (3413 native gameplay/performance checks; Unreal runtime is not covered).
- `git diff --check`: PASS.
- UHT/UBT, Unreal automation, PIE, and packaging still require UE 5.7 on the user's machine. Do not reuse build success from the prior source revision as evidence for this change.

## UE5.7 target settings correction — 2026-10-05

User build output reported MSB3073/code6 and UE5.7 upgrade warnings; the root build error was not included. Changed Game and Editor targets from BuildSettingsVersion.V5 to V6 to align with the engine's requested defaults. Static source validation and diff check passed. This fixes the known old settings; it does not establish that code6 is resolved. UHT/UBT confirmation still requires the user's full Build Output.

## User Windows build evidence and C4458 correction — 2026-10-05

- User supplied UE5.7.1 UBT output: VS2022 MSVC14.44 and Windows SDK10.0.22621 detected; UHT processed the project successfully (28 files written). That attempt stopped before C++ compilation because Live Coding was active. This is user-provided evidence, not an engine run in our environment.
- After closing Editor, user reported eight C4458 errors in HBPlayerController.cpp: local Player declarations hide the inherited APlayerController member. Renamed all eight local variables and their references to PlayerCharacter; no gameplay/API change and no warning suppression.
- Static source validation and git diff whitespace check rerun: PASS. Unreal C++ compilation/linking, PIE and packaging remain pending; wait for the user's next build result.
