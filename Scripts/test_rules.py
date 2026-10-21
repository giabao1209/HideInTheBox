"""Compile and execute the actual portable gameplay rules, without Unreal stubs."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = os.environ.get("CXX") or shutil.which("g++") or shutil.which("clang++")
if not compiler:
    raise SystemExit("No g++/clang++ found. Use Unreal Automation for rules or install a C++17 compiler.")
with tempfile.TemporaryDirectory(prefix="hideinthebox_rules_") as folder:
    binary = Path(folder) / ("rules.exe" if os.name == "nt" else "rules")
    command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic", "-I", str(root / "Source/HideInTheBox/Public"), str(root / "Tests/rules_test.cpp"), "-o", str(binary)]
    if os.name != "nt":
        command[1:1] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(command, check=True)
    subprocess.run([str(binary)], check=True)
