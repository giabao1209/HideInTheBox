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
