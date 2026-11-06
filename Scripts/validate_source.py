"""Static handoff checks only. This does not compile or execute Unreal code."""
from pathlib import Path
import json
import re
import sys

root = Path(__file__).resolve().parents[1]
errors = []

def require(condition, message):
    if not condition:
        errors.append(message)

project = json.loads((root / "HideInTheBox.uproject").read_text())
association = project["EngineAssociation"]
require(association == "5.8" or re.fullmatch(r"\{[0-9A-Fa-f]{8}(?:-[0-9A-Fa-f]{4}){3}-[0-9A-Fa-f]{12}\}", association),
        "Expected UE 5.8 or a registered source-build GUID (see GAS demo guide)")
module = project["Modules"][0]["Name"]
require((root / f"Source/{module}/{module}.Build.cs").is_file(), "Missing runtime Build.cs")
for target in [module, module + "Editor"]:
    require((root / f"Source/{target}.Target.cs").is_file(), f"Missing target {target}")
require(any(p["Name"] == "EnhancedInput" and p["Enabled"] for p in project["Plugins"]), "EnhancedInput not enabled")
for gas_plugin in ("GameplayAbilities",):
    require(any(p["Name"] == gas_plugin and p["Enabled"] for p in project["Plugins"]), f"{gas_plugin} not enabled")
build_rules = (root / f"Source/{module}/{module}.Build.cs").read_text()
for gas_module in ("GameplayAbilities", "GameplayTags", "GameplayTasks"):
    require(f'"{gas_module}"' in build_rules, f"Missing GAS module dependency {gas_module}")
for gas_source in ("HBAttributes.h", "HBEffects.h", "HBGameplayAbility.h"):
    require((root / "Source" / module / "Public" / gas_source).is_file(), f"Missing GAS source {gas_source}")
require((root / "Config/DefaultGameplayTags.ini").is_file(), "Missing GAS cooldown tag config")
source = root / "Source" / module
headers = list(source.rglob("*.h"))
header_names = {p.name for p in headers}
for path in list(source.rglob("*.cpp")) + headers:
    text = path.read_text()
    includes = re.findall(r'^#include\s+"([^"]+)"', text, re.M)
    for include in includes:
        if include.startswith("HB") and not include.endswith(".generated.h"):
            require(include in header_names, f"{path.name}: missing project include {include}")
    if path.suffix == ".h":
        require(text.startswith("#pragma once"), f"{path.name}: missing include guard")
        if re.search(r'\bU(CLASS|STRUCT|ENUM)\(', text):
            expected = path.stem + ".generated.h"
            require(includes and includes[-1] == expected, f"{path.name}: generated header must be last")
    require("LogTemp" not in text, f"{path.name}: use project log category")
    require(not re.search(r'\[[^\]\n]*\]\s*\([^;]*\)\s*(?:mutable\s*)?\{', text), f"{path.name}: lambda violates coding guide")
    require("UnrealEd" not in text, f"{path.name}: runtime depends on editor")
    require(text.count("{") == text.count("}"), f"{path.name}: unbalanced braces")

for path in root.rglob("*.md"):
    text = path.read_text()
    require(text.count("```") % 2 == 0, f"{path.name}: unclosed code fence")
    for target in re.findall(r'\[[^\]]+\]\(([^)]+)\)', text):
        if not target.startswith(("https://", "http://", "#")):
            require((path.parent / target.split("#")[0]).exists(), f"{path.name}: broken local link {target}")
for folder in ("Binaries", "Intermediate", "Saved"):
    # These folders may exist on a developer machine; ignore via git, never publish them.
    require(folder + "/" in (root / ".gitignore").read_text(), f"Missing ignore for {folder}")
require("WalkableFloor" in (root / "Config/DefaultEngine.ini").read_text(), "Missing floor trace channel")
require("EnhancedPlayerInput" in (root / "Config/DefaultInput.ini").read_text(), "Wrong player input class")
if errors:
    print("STATIC CHECK FAILED\n" + "\n".join(errors))
    sys.exit(1)
print(f"STATIC CHECK PASS: {len(headers)} headers, project targets/config, project includes, source hygiene and local documentation links.")
print("UHT/UBT, engine automation, PIE, hardware input and packaged runtime: NOT RUN by this script.")
