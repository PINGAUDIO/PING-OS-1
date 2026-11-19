# Builds the PING FM-1 firmware inside a Linux container (needs Docker Desktop running):
#   powershell -ExecutionPolicy Bypass -File dev\fwbuild.ps1      ->  app\build\*.fwsc, log in dev\out\fw-build.log
$root = Split-Path -Parent $PSScriptRoot
$log = "$root\dev\out\fw-build.log"
New-Item -ItemType Directory -Force "$root\dev\out" | Out-Null
$tc = "$env:USERPROFILE\.jieli\jieli-linux-toolchains-20260730.1"
$sdk = "$env:USERPROFILE\.jieli\ac79-sdk"
# The build's own image: Debian with the Python it needs, made once and kept on this PC. (Each build used to download
# Python afresh, and failed with "python3: not found" whenever that download did not come through.)
$img = "ping-fwbuild:1"
$ilog = Join-Path $root "dev/out/fw-image.log"
docker image inspect $img *> $null
if ($LASTEXITCODE -ne 0) {
    "FROM debian:bookworm-slim`nRUN apt-get update -qq && apt-get install -y -qq python3 python3-pil libraqm0 && rm -rf /var/lib/apt/lists/*`n" |
        docker build --platform linux/amd64 -t $img - *> $ilog
    if ($LASTEXITCODE -ne 0) { Get-Content $ilog -Tail 20; Write-Host "The build image could not be made (it needs the internet once)."; exit 1 }
}
docker run --rm --platform linux/amd64 -v "${root}:/work" -v "${tc}:/opt/jieli:ro" -v "${sdk}:/sdk:ro" -w /work `
  -e JIELI_DOCKER=0 -e JIELI_TOOLCHAIN=/opt/jieli -e AC79_SDK=/sdk $img `
  python3 base/tools/build.py app $args *> $log
$code = $LASTEXITCODE
Get-Content $log -Tail 60
if ($code -eq 0) { python "$root\tools\make_installer.py" }   # the installer page serves the new package at once
exit $code
