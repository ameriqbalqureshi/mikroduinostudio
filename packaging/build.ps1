# Builds the onedir PyInstaller bundle for MikroDuino Studio.
# Run from anywhere; always resolves paths relative to the repo root.
#
# Usage:  powershell -ExecutionPolicy Bypass -File packaging\build.ps1
# (Default PowerShell execution policy blocks unsigned scripts; -ExecutionPolicy
# Bypass only affects this one process, not the system-wide policy.)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

Write-Host "Building MikroDuino Studio (PyInstaller onedir)..." -ForegroundColor Cyan
pyinstaller packaging/mikroduino.spec --distpath dist --workpath build --noconfirm

if ($LASTEXITCODE -ne 0) {
    throw "PyInstaller build failed with exit code $LASTEXITCODE"
}

Write-Host "Build complete: dist\MikroDuinoStudio\MikroDuinoStudio.exe" -ForegroundColor Green
