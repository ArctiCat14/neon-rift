#requires -Version 5.1
<#
    Installs the built NeonRift.vst3 into the system VST3 folder so FL Studio
    can find it. Relaunches itself elevated if needed.
#>
[CmdletBinding()]
param(
    [string] $Vst3Dir = (Join-Path $env:ProgramFiles 'Common Files\VST3')
)

$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'dist\NeonRift.vst3'

if (-not (Test-Path $source)) {
    throw "dist\NeonRift.vst3 not found. Run tools\build.ps1 first."
}

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
$elevated = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $elevated) {
    Write-Host "Requesting administrator rights to write to $Vst3Dir ..." -ForegroundColor Yellow
    $ps = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $args = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Vst3Dir `"$Vst3Dir`""
    Start-Process -FilePath $ps -ArgumentList $args -Verb RunAs
    return
}

$target = Join-Path $Vst3Dir 'NeonRift.vst3'

Write-Host "Installing to $target" -ForegroundColor Cyan
Remove-Item -Recurse -Force $target -ErrorAction SilentlyContinue
Copy-Item -Recurse $source $target

Write-Host "`nInstalled. In FL Studio:" -ForegroundColor Green
Write-Host "  1. Options > Manage plugins"
Write-Host "  2. Enable 'Verify plugins', press 'Find more plugins'"
Write-Host "  3. NeonRift appears under Installed > Generators (VST3)"
