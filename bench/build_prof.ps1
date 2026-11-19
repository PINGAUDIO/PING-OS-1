# Builds the native PC bench with MSVC (Build Tools 2022).
# The compiler environment is captured once into %USERPROFILE%\.chisel-tools\msvc-env.txt.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $PSScriptRoot 'build_prof'
New-Item -ItemType Directory -Force $out | Out-Null
$envFile = Join-Path $env:USERPROFILE '.chisel-tools\msvc-env.txt'
if (!(Test-Path $envFile)) {
    $vc = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
    cmd /c "`"$vc`" >nul 2>&1 && set" | Where-Object { $_ -match '^(PATH|INCLUDE|LIB|LIBPATH)=' } | Set-Content -Encoding utf8 $envFile
}
foreach ($line in Get-Content $envFile) { if ($line -match '^([A-Z]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] } }
$src = @("$root\bench\bench.c") + (Get-ChildItem "$root\engine\*.c" | ForEach-Object FullName)
& cl.exe /nologo /O2 /fp:fast /W3 /D_CRT_SECURE_NO_WARNINGS /DCHISEL_HOST /DCH_PROF "/I$root\engine" "/Fo$out\" "/Fe$out\bench_prof.exe" @src
if ($LASTEXITCODE -ne 0) { throw "bench build failed" }
