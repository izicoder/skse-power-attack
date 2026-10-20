#requires -Version 7.0
<#
.SYNOPSIS
	Builds the plugin and packages an installable archive in packages/.

.DESCRIPTION
	Configures and builds the vs2022 preset with the requested version stamped into the binary
	(-DSPA_VERSION), then stages the mod the way a mod manager expects to install it and writes
	packages/SKSEPowerAttack-<Version>.zip. Nothing is deployed to a mod folder and nothing is
	uploaded: installing the zip is a deliberate action.

	The archive is laid out as:

		SKSE/Plugins/SKSEPowerAttack.dll
		docs/README.md                        (documentation; "docs" is ignored by the managers)

	The plugin writes SKSEPowerAttack.json next to the DLL on first run. No config file is shipped
	on purpose: shipping one would reset the settings of an existing install on every update.

	The build needs the MSVC environment a "developer shell" provides, but starting this from a
	normal shell is fine: vcvars64.bat is located with vswhere and imported into this process.

.PARAMETER Version
	Version for this test/release, e.g. 1.0.1. It is stamped into the DLL (SPA_VERSION, so the log
	line and the SKSE plugin record carry it) and it names the archive.

.PARAMETER SkipBuild
	Package what is already in the build tree, without building. The DLL keeps whatever version it
	was last built with.

.PARAMETER FromFolder
	Stage SKSE/Plugins/*.dll from this installed mod folder instead of the build tree - the honest
	archive of what the game actually runs. Nothing is built or deployed by this option, so pair it
	with -SkipBuild (otherwise the build runs first and the folder is still what gets zipped).

.PARAMETER Output
	Archive to write. Defaults to packages/SKSEPowerAttack-<Version>.zip next to the repo root.

.EXAMPLE
	.\tools\Build-Release.ps1 -Version 0.1.0

.EXAMPLE
	.\tools\Build-Release.ps1 -Version 0.1.0 -SkipBuild
#>
[CmdletBinding()]
param(
	[string]$Version = '1.0.0',
	[switch]$SkipBuild,
	[string]$FromFolder,
	[string]$Output
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

if ($Version -notmatch '^\d+(\.\d+){1,3}$') {
	throw "-Version must look like 1.0.1, got '$Version'."
}
if (-not $Output) { $Output = Join-Path $root "packages\SKSEPowerAttack-$Version.zip" }

# ---------------------------------------------------------------------------
# Toolchain (MSVC + CMake + Ninja), so this runs from any shell
# ---------------------------------------------------------------------------
function Get-VsInstallPath
{
	$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
	if (-not (Test-Path $vswhere)) { return $null }
	$found = & $vswhere -latest -products * `
		-requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
		-property installationPath
	if ($LASTEXITCODE -ne 0) { return $null }
	return ($found | Select-Object -First 1)
}

function Initialize-MsvcEnvironment
{
	$vsPath = Get-VsInstallPath
	if (-not $vsPath) {
		# No VS found: assume the caller is already in a developer shell and let it fail loudly
		# at configure time if that is wrong.
		if (Get-Command cl.exe -ErrorAction SilentlyContinue) { return }
		throw 'Visual Studio with the C++ workload not found (vswhere reported nothing).'
	}

	# The IDE's own cmake and ninja are the copies the preset was written against; they are not
	# on PATH outside a developer prompt.
	$env:PATH = @(
		(Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'),
		(Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja'),
		$env:PATH
	) -join ';'

	$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
	if (-not (Test-Path $vcvars)) { throw "No vcvars64.bat at $vcvars." }
	cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
		# "=C:=C:\..." lines start with '=' and must not be turned into an environment variable.
		if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($matches[1])" -Value $matches[2] }
	}
}

function Get-ConfigurePreset
{
	# vs2022 holds the shared settings. A git-ignored CMakeUserPresets.json may add a preset that
	# inherits it (a local prebuilt CommonLibSSE path); that is the one to configure with.
	$preset = 'vs2022'
	$userPresets = Join-Path $root 'CMakeUserPresets.json'
	if (Test-Path $userPresets) {
		$inheriting = (Get-Content $userPresets -Raw | ConvertFrom-Json).configurePresets |
			Where-Object { $_.inherits -eq 'vs2022' } |
			Select-Object -First 1
		if ($inheriting) { $preset = $inheriting.name }
	}
	return $preset
}

function Get-BinaryDir
{
	$configure = (Get-Content (Join-Path $root 'CMakePresets.json') -Raw | ConvertFrom-Json).
		configurePresets | Where-Object { $_.name -eq 'vs2022' } | Select-Object -First 1
	if (-not $configure.binaryDir) { throw 'The vs2022 preset has no binaryDir.' }
	return $configure.binaryDir.Replace('${sourceDir}', $root).Replace('/', '\')
}

function Invoke-Native
{
	param([string]$Command, [string[]]$Arguments)
	& $Command @Arguments
	if ($LASTEXITCODE -ne 0) { throw "$Command $($Arguments -join ' ') failed with exit code $LASTEXITCODE." }
}

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
$binaryDir = Get-BinaryDir
$dll = Join-Path $binaryDir 'SKSEPowerAttack.dll'

if (-not $SkipBuild) {
	Initialize-MsvcEnvironment

	# The preset interpolates $env:VCPKG_ROOT, so it must be set before configure runs.
	if (-not $env:VCPKG_ROOT) {
		$local = Join-Path $root '.vcpkg'
		if (Test-Path (Join-Path $local 'scripts\buildsystems\vcpkg.cmake')) { $env:VCPKG_ROOT = $local }
		else { throw 'vcpkg not found: set VCPKG_ROOT or clone one into .vcpkg.' }
	}

	$preset = Get-ConfigurePreset
	Write-Host "Configuring $preset with version $Version"
	Invoke-Native cmake @('--preset', $preset, "-DSPA_VERSION=$Version")

	Write-Host 'Building'
	Invoke-Native cmake @('--build', $binaryDir)
}

# ---------------------------------------------------------------------------
# Stage and zip
# ---------------------------------------------------------------------------
# Stage in a scratch folder and zip that: the archive must have SKSE/Plugins at its root.
$stage = Join-Path $root 'packages\.staging'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
$plugins = Join-Path $stage 'SKSE\Plugins'
New-Item -ItemType Directory -Path $plugins -Force | Out-Null

if (-not (Test-Path $dll) -and -not $FromFolder) { throw "No DLL at $dll - build it (drop -SkipBuild), or pass -FromFolder." }
if ($FromFolder) {
	$source = (Resolve-Path (Join-Path $FromFolder 'SKSE\Plugins')).Path
	Copy-Item (Join-Path $source '*.dll') $plugins
	# Only the DLL ships. SKSEPowerAttack.json in that folder is the user's own config: a copy in
	# the archive would replace a tuned file on every update.
	Write-Host "Staged from $source"
} else {
	Copy-Item $dll $plugins
	Write-Host "Staged from $dll"
}

# README goes in a docs/ folder: MO2 ignores a root docs folder, so the archive's data root stays
# exactly SKSE/Plugins.
$docs = Join-Path $stage 'docs'
New-Item -ItemType Directory -Path $docs -Force | Out-Null
Copy-Item (Join-Path $root 'README.md') $docs

New-Item -ItemType Directory -Path (Split-Path $Output) -Force | Out-Null
if (Test-Path $Output) { Remove-Item $Output -Force }

# ZipFile (not Compress-Archive) so the entries use forward slashes, which is what the mod
# managers and the Nexus scanners expect to see.
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory(
	$stage,
	(Join-Path (Resolve-Path (Split-Path $Output)).Path (Split-Path $Output -Leaf)),
	[System.IO.Compression.CompressionLevel]::Optimal,
	$false
)

Remove-Item $stage -Recurse -Force

$zip = Get-Item $Output
Write-Host ("Packaged {0} ({1:N0} bytes)" -f $zip.FullName, $zip.Length)
Write-Output $zip.FullName
