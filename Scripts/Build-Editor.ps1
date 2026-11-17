param(
    [Parameter(Mandatory=$true)][string]$EngineRoot
)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$Project = Join-Path $ProjectRoot 'HideInTheBox.uproject'
$Build = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
if (!(Test-Path $Build)) { throw "Build.bat not found under EngineRoot: $EngineRoot" }
& $Build HideInTheBoxEditor Win64 Development "-Project=$Project" -Module=HideInTheBox -WaitMutex -NoEngineChanges -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw "Unreal build failed with exit code $LASTEXITCODE" }
Write-Host 'Game editor module built using the existing engine. Open /Game/Maps/GASWidgetArena in the editor to try the practice arena.'
