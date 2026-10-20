# Manual deploy: copies the built DLL into an MO2 mod folder.
#
# Nothing in the build does this on its own - deploying is a deliberate test step, and it must not
# happen while the game is running (the running process has the DLL loaded).
#
#   .\tools\Deploy.ps1                          # -> C:\Modding\MO2\mods\SKSE Power Attack
#   .\tools\Deploy.ps1 -Path "D:\mods\My Test"  # any mod folder
param(
	[string]$Path = "C:\Modding\MO2\mods\SKSE Power Attack",
	[switch]$SkipGameCheck
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$dll = Join-Path $root "build\vs2022\SKSEPowerAttack.dll"

if (-not (Test-Path $dll)) {
	throw "No build found at $dll - run cmake --build build\vs2022 first."
}

if (-not $SkipGameCheck) {
	$running = Get-Process -Name SkyrimSE -ErrorAction SilentlyContinue
	if ($running) {
		throw "Skyrim is running (PID $($running.Id)) - close it first, the DLL is loaded and would be replaced under it."
	}
}

$plugins = Join-Path $Path "SKSE\Plugins"
New-Item -ItemType Directory -Force -Path $plugins | Out-Null
Copy-Item $dll $plugins -Force

$target = Join-Path $plugins "SKSEPowerAttack.dll"
Write-Host "Deployed $(Split-Path -Leaf $dll) -> $target"
Get-Item $target | Select-Object Length, LastWriteTime | Format-List
