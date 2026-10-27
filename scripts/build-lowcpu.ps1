<#
.SYNOPSIS
    Builds Prism Launcher with a limited number of compile jobs.

.DESCRIPTION
    Runs cmake --build with --parallel <Jobs> so the CPU does not stay at 100%.
    With -AddDefenderExclusion the build directory is added to Windows Defender exclusions.

.EXAMPLE
    .\scripts\build-lowcpu.ps1
    .\scripts\build-lowcpu.ps1 -Jobs 2 -Preset windows_msvc -AddDefenderExclusion
#>

param(
    [int]    $Jobs = 4,
    [string] $Preset = "",
    [string] $BuildDir = "",
    [string] $Config = "",
    [switch] $AddDefenderExclusion
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $BuildDir) { $BuildDir = Join-Path $repoRoot 'build' }

if ($AddDefenderExclusion) {
    $exclusionPath = [System.IO.Path]::GetFullPath($BuildDir)
    try {
        $existing = (Get-MpPreference).ExclusionPath
        if ($existing -notcontains $exclusionPath) {
            Add-MpPreference -ExclusionPath $exclusionPath
            Write-Host "Added to Windows Defender exclusions: $exclusionPath"
        } else {
            Write-Host "Already excluded in Windows Defender: $exclusionPath"
        }
    } catch {
        Write-Warning "Could not add the Defender exclusion (administrator rights required): $($_.Exception.Message)"
    }
}

if (-not (Test-Path -LiteralPath (Join-Path $BuildDir 'CMakeCache.txt'))) {
    if (-not $Preset) {
        $Preset = if ($env:COMPILER -eq 'msvc' -or $env:VSCMD_VER) { 'windows_msvc' } else { 'windows_mingw' }
    }
    if (-not $env:VCPKG_ROOT -and (Test-Path -LiteralPath 'C:\vcpkg')) {
        $env:VCPKG_ROOT = 'C:\vcpkg'
    }
    if ($Preset -eq 'windows_mingw') {
        $mingwBin = @(
            'C:\msys64\ucrt64\bin',
            'C:\msys64\mingw64\bin',
            'C:\msys64\clang64\bin',
            'C:\tools\msys64\ucrt64\bin',
            'C:\mingw64\bin',
            'C:\ProgramData\mingw64\mingw64\bin'
        ) | Where-Object { Test-Path -LiteralPath (Join-Path $_ 'g++.exe') } | Select-Object -First 1
        if (-not $mingwBin) {
            $mingwBin = (Get-ChildItem 'C:\msys64' -Directory -ErrorAction SilentlyContinue |
                ForEach-Object { Join-Path $_.FullName 'bin' } |
                Where-Object { Test-Path -LiteralPath (Join-Path $_ 'g++.exe') } |
                Select-Object -First 1)
        }
        if (-not $mingwBin) {
            throw "MinGW was not found. Install MSYS2/MinGW-w64 or rerun with -Preset windows_msvc from a Developer Prompt."
        }
        $env:PATH = "$mingwBin;$env:PATH"
        Write-Host "MinGW: $mingwBin"
    }
    Write-Host "Configure: cmake --preset $Preset"
    Push-Location -LiteralPath $repoRoot
    try {
        & cmake --preset $Preset
    } finally {
        Pop-Location
    }
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE" }
}

$buildArgs = @('--build', $BuildDir, '--parallel', "$Jobs")
if ($Config) { $buildArgs += @('--config', $Config) }

Write-Host "Build: cmake $($buildArgs -join ' ')"
Push-Location -LiteralPath $repoRoot
try {
    & cmake @buildArgs
} finally {
    Pop-Location
}
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }

Write-Host "Build finished successfully (jobs: $Jobs)."
