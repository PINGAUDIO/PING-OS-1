# Builds PING OS-1 for the browser (WebAssembly) with Emscripten in Docker:
#   powershell -ExecutionPolicy Bypass -File sim\web\build.ps1     ->  app\web\play\ping-os-1.wasm
# The fonts come from the same generators the Windows simulator uses (sim\gen).
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$gen = Join-Path $root 'sim\gen'
$out = Join-Path $root 'app\web\play'
New-Item -ItemType Directory -Force $gen, $out | Out-Null
if (!(Test-Path "$gen\felucca_font.h")) { python "$root\sim\gen_font_wrap.py" "$root\base\tools\gen_font.py" "$gen\felucca_font.h" }
python "$root\app\gen_wordmark.py" "$gen"
if ($LASTEXITCODE -ne 0) { throw "wordmark fonts failed" }
$exports = '_web_init,_web_process,_web_out,_web_fb,_web_fb_take,_web_lights,_web_input,_web_turn,_web_master,_web_midi,_web_splash,_web_alloc,_web_free,_web_store_put'
$cmd = "emcc -O2 -ffast-math -DCHISEL_HOST -Wno-everything -Iengine -Iapp -Ibase/include -Ibase/device -Isim/gen " +
       "sim/web/ping_web.c engine/*.c -o app/web/play/ping-os-1.wasm --no-entry " +
       "-sEXPORTED_FUNCTIONS=$exports -sERROR_ON_UNDEFINED_SYMBOLS=0 -sINITIAL_MEMORY=67108864 -sSTACK_SIZE=1048576"
docker run --rm -v "${root}:/src" -w /src emscripten/emsdk:3.1.74 sh -c $cmd
if ($LASTEXITCODE -ne 0) { throw "web build failed" }
Write-Host "built $out\ping-os-1.wasm"
