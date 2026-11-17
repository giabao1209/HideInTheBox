# HideInTheBox

An independent, single-player Unreal Engine prototype based on the supplied Vaultbreaker design. The loop starts at camp, takes you through a dungeon for Stones, and brings you back to build defenses and protect the Anima.

## Open the project

This branch uses **Unreal Engine 5.8.4 (Vault Engine source build)**. Associate `HideInTheBox.uproject` with that engine build on your machine, open it in the Unreal Editor, and press **Play**.

| Map | What to try |
| --- | --- |
| `DemoArena` | The default map. Play the full camp, dungeon, extraction, prep, and defense loop. |
| `GASWidgetArena` | A practice room with three stationary targets for checking skill damage, mana costs, cooldowns, and cast interruptions. |

## What’s in the prototype

- **Dungeon and defense.** Clear the main route and defeat the elite. Optional caches give extra Stones and raise Threat. Extract back to camp, place or upgrade turrets, build fences, and repair the Gate. Keep the Anima alive through five defense waves.
- **Combat and skills.** The Unreal **Gameplay Ability System (GAS)** drives Basic, Pulse, Dodge, Guard, Mend, and Bolt. Gameplay Effects handle health changes, mana costs, and cooldowns. Pulse marks surviving targets as **Exposed**. Bolt consumes the mark for bonus damage and can interrupt the elite’s Slam.
- **Inventory and loadout.** Spend Stones to upgrade skills and swap equipment or assign skills to slots during Town and Prep.
- **UI and input.** The gameplay HUD, inventory, and pause menu use **UMG Widget Blueprints with CommonUI**. The HUD shows health, mana, skill slots, cooldowns, and input prompts. **Enhanced Input** handles keyboard and mouse plus gamepad controls.

## Controls

| Action | Keyboard and mouse | Xbox-style gamepad |
| --- | --- | --- |
| Move / aim | **WASD** or left-click the ground to move. Aim with the cursor. | Left stick / right stick |
| Basic attack | **Left-click** an enemy | **A** |
| Pulse | **Right-click** where you want Pulse to land | **B** |
| Dodge / Guard / Mend / Bolt | **1 / 2 / 3 / 4**. Aim at an enemy before casting Bolt. | **X / Y / LB / RB** |
| Enter the dungeon, extract, or start Defense | **Enter**. For extraction, clear the main route and stand near the blue crystal. | **D-pad Up** |
| Open a cache / inventory | **E / I** | **L3 / RT** |
| Build mode / cycle tile / repair | **B / Tab / R** | **D-pad Down / Right / Left** |
| Cancel / pause | **Space / P** | **R3 / Start** |

In keyboard-and-mouse build mode, point at a tile and **left-click** to place or upgrade a structure. **Right-click** switches between turret and fence, and **1** rotates the fence. Outside build mode, **R** repairs the Gate during Prep.

## Debug and practice

| Shortcut | Action |
| --- | --- |
| **F1 / F3** | Unreal’s default debug view shortcuts for Wireframe and Lit, when editor debug bindings are enabled. |
| **F2 / LT** | During an unpaused `DemoArena` run, open the Arena Test Panel to adjust Stones, health, and mana. Press again to cycle **State → Events → Config Checks → Hide**. The practice room starts at **State**. |
| **F5** | Restart the current map. |
| **F6** | Restore health and mana and clear cooldowns. In `GASWidgetArena`, also respawn the three targets. |
| **F7** | Take a test hit. Try it while Guard is active or during Pulse windup. |
| **F8** | Refill mana while keeping current cooldowns. |

The **State** page shows FPS, frame time, and current gameplay state. **Events** shows recent input, animation, and combat events. **Config Checks** reports setup checks. In the Unreal console, `HBDebugCheck` reruns the checks and `HBDebugDump` writes diagnostics and event history to the Output Log.

For a quick GAS check, open `GASWidgetArena`, hit a target with **Pulse**, then follow up with **Bolt** before **Exposed** expires. Press **F6** to repeat.
