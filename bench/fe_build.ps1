# Builds the Felucca-engine test bench (bench/fe_test.c + engine/ch_fe*.c) with MSVC (Build Tools 2022) and runs it:
#   powershell -ExecutionPolicy Bypass -File bench\fe_build.ps1      -> bench\build\fe_test.exe, WAVs in dev\out\fe
# The compiler environment is captured once into %USERPROFILE%\.chisel-tools\msvc-env.txt (as bench\build.ps1).
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $PSScriptRoot 'build'
$obj = Join-Path $out 'fe'
New-Item -ItemType Directory -Force $obj | Out-Null
$envFile = Join-Path $env:USERPROFILE '.chisel-tools\msvc-env.txt'
if (!(Test-Path $envFile)) {
    New-Item -ItemType Directory -Force (Split-Path $envFile) | Out-Null
    $vc = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
    cmd /c "`"$vc`" >nul 2>&1 && set" | Where-Object { $_ -match '^(PATH|INCLUDE|LIB|LIBPATH)=' } | Set-Content -Encoding utf8 $envFile
}
foreach ($line in Get-Content $envFile) { if ($line -match '^([A-Z]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }
$src = @("$root\bench\fe_test.c") + (Get-ChildItem "$root\engine\ch_fe*.c" | ForEach-Object FullName)
& cl.exe /nologo /O2 /W4 /D_CRT_SECURE_NO_WARNINGS /DCHISEL_HOST "/I$root\engine" "/I$root\bench" "/Fo$obj\" "/Fe$out\fe_test.exe" @src
if ($LASTEXITCODE -ne 0) { throw "fe_test build failed" }
Push-Location $root
try { & "$out\fe_test.exe"; $code = $LASTEXITCODE } finally { Pop-Location }
exit $code
