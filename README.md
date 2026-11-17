# Hide In The Box

A single-player Unreal Engine 5.8.4 demo built with the Gameplay Ability System. Fight through a dungeon, bring Stones back to camp, and defend the Anima.

## Run the demo

Open `HideInTheBox.uproject` with Vault Engine 5.8.4. In the Unreal Editor, open one of these maps and press **Play**.

- **`/Game/Maps/DemoArena`** is the full dungeon and defense loop.
- **`/Game/Maps/GASWidgetArena`** is a practice room with three stationary targets for trying skills, cooldowns, and mana costs.

## DemoArena walkthrough

1. Press **Enter** at camp to enter the dungeon. Clear the main route and defeat the elite. Optional caches give extra Stones and increase Threat.
2. Go to the blue extraction crystal at the end of the route and press **Enter** nearby to return to camp.
3. During Prep, press **B** to build. Aim at a tile, use **right-click** to switch between turrets and fences, and **left-click** to place. Use **R** to repair a selected structure or, outside build mode, the Gate.
4. Press **Enter** to start Defense. Keep the Anima alive through five waves.

## Keyboard and mouse

| Input | Action |
| --- | --- |
| **WASD** or **left-click** on the ground | Move |
| **Left-click** an enemy | Basic attack |
| **Right-click** during combat | Pulse, which marks surviving enemies as **Exposed** |
| **1 / 2 / 3** | Dodge / Guard / Mend |
| **4** while aiming at an enemy | Fire Bolt. It deals bonus damage to **Exposed** enemies and consumes the mark. |
| **E** near an unlocked cache | Open the cache |
| **I** | Open inventory. Change equipment or upgrade skills at camp during Town or Prep. |
| **F6 / F7 / F8** | Reset health, mana, and cooldowns / take a test hit / refill mana |

In the practice room, hit a target with **Pulse**, then follow up with **Bolt** before **Exposed** expires. Use **F6** to reset the targets and repeat.
