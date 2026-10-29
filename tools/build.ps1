#requires -Version 5.1
<#
    Builds NeonRift (VST3 + Standalone) and the offline render test.

    Usage:
        powershell -File tools\build.ps1                 # release build
        powershell -File tools\build.ps1 -Clean          # wipe build dir first
        powershell -File tools\build.ps1 -Config Debug   # debug build
#>
[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')]
    [string] $Config = 'Release',

    [switch] $Clean
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root 'build-nmake'
$distDir = Join-Path $root 'dist'

Push-Location $root

try {
    # -- locate the MSVC environment ------------------------------------------
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found. Is Visual Studio installed?"
    }

    $vsPath = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath

    if (-not $vsPath) {
        throw "No Visual Studio installation with the C++ toolset was found."
    }

    $vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'

    if (-not (Test-Path $vcvars)) {
        throw "vcvars64.bat not found at $vcvars"
    }

    Write-Host "MSVC: $vsPath" -ForegroundColor Cyan
    Write-Host "Config: $Config" -ForegroundColor Cyan

    if ($Clean -and (Test-Path $buildDir)) {
        Write-Host "Cleaning $buildDir" -ForegroundColor Yellow
        Remove-Item -Recurse -Force $buildDir
    }

    # NOTE: the NMake Makefiles generator is used instead of Ninja. Ninja's
    # child-process handling deadlocks inside this sandbox (its stdout pipes
    # are blocked), while nmake drives cl.exe directly and works fine.
    $configure = 'call "{0}" >nul && cmake -S "{1}" -B "{2}" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE={3}' -f `
        $vcvars, $root, $buildDir, $Config

    Write-Host "`n== Configuring ==" -ForegroundColor Cyan
    & cmd.exe /c $configure

    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed (exit $LASTEXITCODE)"
    }

    Write-Host "`n== Building ==" -ForegroundColor Cyan
    & cmd.exe /c ('call "{0}" >nul && cmake --build "{1}" --config {2}' -f $vcvars, $buildDir, $Config)

    if ($LASTEXITCODE -ne 0) {
        throw "Build failed (exit $LASTEXITCODE)"
    }

    # -- collect artefacts ----------------------------------------------------
    New-Item -ItemType Directory -Force $distDir | Out-Null

    $vst3 = Get-ChildItem $buildDir -Recurse -Filter 'NeonRift.vst3' -Directory -ErrorAction SilentlyContinue |
                Select-Object -First 1
    $standalone = Get-ChildItem $buildDir -Recurse -Filter 'NeonRift.exe' -File -ErrorAction SilentlyContinue |
                Where-Object { $_.FullName -notmatch 'NeonRiftRender' } | Select-Object -First 1

    if ($vst3) {
        $target = Join-Path $distDir 'NeonRift.vst3'
        Remove-Item -Recurse -Force $target -ErrorAction SilentlyContinue
        Copy-Item -Recurse $vst3.FullName $target
        Write-Host "VST3      -> $target" -ForegroundColor Green
    } else {
        Write-Warning "NeonRift.vst3 was not produced"
    }

    if ($standalone) {
        $target = Join-Path $distDir 'NeonRift.exe'
        Copy-Item -Force $standalone.FullName $target
        Write-Host "Standalone-> $target" -ForegroundColor Green
    } else {
        Write-Warning "Standalone executable was not produced"
    }

    Write-Host "`nDone." -ForegroundColor Green
}
finally {
    Pop-Location
}
