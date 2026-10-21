param(
    [Parameter(Mandatory=$true)][string]$EngineRoot
)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$Project = Join-Path $ProjectRoot 'HideInTheBox.uproject'
$Build = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
if (!(Test-Path $Build)) { throw "Build.bat not found under EngineRoot: $EngineRoot" }
& $Build HideInTheBoxEditor Win64 Development "-Project=$Project" -WaitMutex
if ($LASTEXITCODE -ne 0) { throw "Unreal build failed with exit code $LASTEXITCODE" }
Write-Host 'Editor target built. Continue with Docs/EDITOR_SETUP_VI.md and runtime tests.'
