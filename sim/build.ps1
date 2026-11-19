# Builds the PING FM-1 simulator for Windows with MSVC (Build Tools 2022):
#   powershell -ExecutionPolicy Bypass -File sim\build.ps1     ->  sim\build\PING-OS-1-sim.exe
# The font header comes from fm1-base's tools/gen_font.py (via gen_font_wrap.py; needs Pillow).
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $PSScriptRoot 'build'
$gen = Join-Path $PSScriptRoot 'gen'
New-Item -ItemType Directory -Force $out, $gen | Out-Null
if (!(Test-Path "$gen\felucca_font.h")) { python "$PSScriptRoot\gen_font_wrap.py" "$root\base\tools\gen_font.py" "$gen\felucca_font.h" }
python "$root\app\gen_wordmark.py" "$gen"
if ($LASTEXITCODE -ne 0) { throw "wordmark fonts failed" }
$envFile = Join-Path $env:USERPROFILE '.chisel-tools\msvc-env.txt'
if (!(Test-Path $envFile)) {
    $vc = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
    cmd /c "`"$vc`" >nul 2>&1 && set" | Where-Object { $_ -match '^(PATH|INCLUDE|LIB|LIBPATH)=' } | Set-Content -Encoding utf8 $envFile
}
foreach ($line in Get-Content $envFile) { if ($line -match '^([A-Z]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }
$src = @("$PSScriptRoot\ping_sim.c") + (Get-ChildItem "$root\engine\*.c" | ForEach-Object FullName)
& cl.exe /nologo /O2 /fp:fast /W3 /wd4244 /wd4305 /wd4996 /D_CRT_SECURE_NO_WARNINGS /DCHISEL_HOST `
    "/I$root\engine" "/I$root\app" "/I$root\base\include" "/I$root\base\device" "/I$gen" `
    "/Fo$out\" "/Fe$out\PING-OS-1-sim.exe" @src /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib winmm.lib
if ($LASTEXITCODE -ne 0) { throw "simulator build failed" }
Write-Host "built $out\PING-OS-1-sim.exe"
