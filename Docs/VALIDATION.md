# Validation record

Date: 2026-10-04. Source handoff from an environment without Unreal Engine.

## Executed

- `python Scripts/validate_source.py`: PASS, 14 headers; project/targets/config, local project includes, generated-header order, hygiene and documentation links.
- `ASAN_OPTIONS=detect_leaks=0 python Scripts/test_rules.py`: PASS, 3392 checks against the actual portable runtime rule header, compiled using g++ C++17 with warnings as errors and AddressSanitizer/UndefinedBehaviorSanitizer. Coverage: sampled radial dead-zone inputs, wave bounds, mana/cooldown transaction rejection, shared ability cooldown, greed claim idempotency and integer overflow. Most checks are sampled stick inputs, not separate gameplay scenarios.
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
