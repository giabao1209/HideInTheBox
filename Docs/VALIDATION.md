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

## UE5.7 target settings correction — 2026-10-05

User build output reported MSB3073/code6 and UE5.7 upgrade warnings; the root build error was not included. Changed Game and Editor targets from BuildSettingsVersion.V5 to V6 to align with the engine's requested defaults. Static source validation and diff check passed. This fixes the known old settings; it does not establish that code6 is resolved. UHT/UBT confirmation still requires the user's full Build Output.

## User Windows build evidence and C4458 correction — 2026-10-05

- User supplied UE5.7.1 UBT output: VS2022 MSVC14.44 and Windows SDK10.0.22621 detected; UHT processed the project successfully (28 files written). That attempt stopped before C++ compilation because Live Coding was active. This is user-provided evidence, not an engine run in our environment.
- After closing Editor, user reported eight C4458 errors in HBPlayerController.cpp: local Player declarations hide the inherited APlayerController member. Renamed all eight local variables and their references to PlayerCharacter; no gameplay/API change and no warning suppression.
- Static source validation and git diff whitespace check rerun: PASS. Unreal C++ compilation/linking, PIE and packaging remain pending; wait for the user's next build result.

## Common UI skill demo — 2026-10-12

- Full `HideInTheBoxEditor Win64 Development` UHT/C++/link: PASS on installed Epic UE 5.7.1, MSVC 14.44 and Windows SDK 10.0.22621.0. The exact proposed commit source/config/project were assembled into ignored `Saved/SkillUICommitCheck` so the running UE source Editor and the primary project's binaries were unaffected. This validates the current source with the installed engine; it does not build the primary project against `C:\Lambro\UnrealEngine`.
- Fixed HUD compile errors exposed by UBT: unsupported `FAnchors`/`FSlateChildSize` constructors, local variables shadowing `UWidget::Slot`, and added private `SlateCore` dependency for drawing panel brushes.
- `HideInTheBox.Skills.CommonUIButton`: PASS in offscreen standalone at 1280×720 and 1920×1080, process exit code 0. Uses actual native controller/player/HUD and CommonButton click delegates. Checks a successful Guard spends its mana cost and starts cooldown; two slots share that ability's cooldown; a rejected second click neither spends mana nor restarts cooldown; readable feedback reaches the controller.
- Visually inspected offscreen runtime screenshots at both resolutions. Saved previews: `Saved/SkillUIDemo-720p.png` and `Saved/SkillUIDemo-1080p.png`. HUD has six skill cards, dark panel styles, HP/MP, Guard at 5.0s cooldown and MP 85/100; the narrow resource label wraps rather than clipping.
- `python -X utf8 Scripts/validate_source.py`: PASS, 18 headers. UTF-8 mode is needed for Vietnamese documents on this Windows machine. `git diff --check`: PASS.
- NOT RUN: physical mouse click/hit testing, Blueprint DemoArena PIE, gamepad hardware/navigation/focus/disconnect, source-engine build of the primary project, packaging. The automation calls CommonButton delegates directly; it does not synthesize mouse or gamepad events. Earlier validation sections are historical and are superseded only for the checks listed above.
- Standalone test uses `/Engine/Maps/Entry?game=/Script/HideInTheBox.HBGameMode` and native loadout/input mappings. The pre-existing local IMC refactor, animation changes and untracked game assets are excluded from this commit.
