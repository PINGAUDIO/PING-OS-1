# Builds dev\vabench.c against an engine directory (default: this tree's):
#   powershell -ExecutionPolicy Bypass -File dev\vabench.ps1 [-Engine DIR] [-Out EXE]
param([string]$Engine = '', [string]$Out = '', [switch]$NoSub)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (!$Engine) { $Engine = "$root\engine" }
if (!$Out) { $Out = "$root\dev\out\vabench\vabench.exe" }
$dir = Split-Path -Parent $Out
New-Item -ItemType Directory -Force $dir, "$dir\obj" | Out-Null
$envFile = Join-Path $env:USERPROFILE '.chisel-tools\msvc-env.txt'
foreach ($line in Get-Content $envFile) { if ($line -match '^([A-Z]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }
$src = @("$PSScriptRoot\vabench.c") + (Get-ChildItem "$Engine\*.c" | ForEach-Object FullName)
& cl.exe /nologo /O2 /fp:precise /W3 /wd4244 /wd4305 /wd4996 /D_CRT_SECURE_NO_WARNINGS /DCHISEL_HOST $(if ($NoSub) { "/DCH_BENCH_NOSUB" }) $(if ($Engine -notmatch "v1src") { "/DCH_BENCH_UNISON" }) "/I$Engine" "/Fo$dir\obj\" "/Fe$Out" @src
if ($LASTEXITCODE -ne 0) { throw "bench build failed" }
Write-Host "built $Out"
