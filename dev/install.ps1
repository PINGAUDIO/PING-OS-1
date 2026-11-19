# PING OS-1: installs the newest build on the FM-1 over USB with fm1-base's installer, which asks before writing.
# (Desktop shortcut "PING OS-1 - Install".)   powershell -ExecutionPolicy Bypass -File dev\install.ps1 [-Info]
param([switch]$Info)
$root = Split-Path -Parent $PSScriptRoot
# the Python with the MIDI libraries (mido, python-rtmidi), by its full path: "python" alone can be the Store's stand-in
$py = Join-Path $env:LOCALAPPDATA 'Programs\Python\Python310\python.exe'
if (!(Test-Path -LiteralPath $py)) { $py = (Get-Command python -ErrorAction SilentlyContinue).Source }
if ($Info) { & "$PSScriptRoot\check-fm1.ps1"; exit }
$Host.UI.RawUI.WindowTitle = "PING OS-1 - Install"
$pkg = "$root\app\build\ping-os-1.fwsc"
function Done($msg, $color) { Write-Host ""; Write-Host "  $msg" -ForegroundColor $color; Write-Host ""; Read-Host "  Press Enter to close"; exit }

Write-Host ""
Write-Host "  PING OS-1 - install on the FM-1" -ForegroundColor Cyan
Write-Host "  -----------------------------"
Write-Host "  Before you go on:"
Write-Host "   - Turn the FM-1's volume down and take headphones off."
Write-Host "   - Keep the USB cable plugged in until this window says it is done."
Write-Host "   - Close anything else using the FM-1's MIDI (a DAW, M-UPGRADE, a web page)."
Write-Host ""
if (!(Test-Path -LiteralPath $pkg)) { Done "The PING OS-1 package is missing ($pkg). Build it first: devwbuild.ps1 --release 1.0" Yellow }
Write-Host ("  Package: ping-os-1.fwsc, built " + (Get-Item -LiteralPath $pkg).LastWriteTime)
Write-Host ""
Write-Host "  Looking for the FM-1 ..."
$found = & $py "$root\base\tools\fm1_install.py" --info 2>&1 | Out-String
if ($found -match 'needs mido') { Done "Setup problem on this PC (not the FM-1): Python's MIDI libraries are missing for $py. Check the setup with this message." Yellow }
if ($LASTEXITCODE -ne 0 -or -not $found.Trim()) { Done "The FM-1 was not found. Plug it in by USB, switch it on, and open this shortcut again." Yellow }
Write-Host "  Found: $($found.Trim())" -ForegroundColor Green
Write-Host ""
Write-Host "  The installer will now show what it is about to do and ask you to type  y  then Enter."
Write-Host "  Anything else cancels and nothing is written."
Write-Host ""
& $py "$root\base\tools\fm1_install.py" "$pkg"
switch ($LASTEXITCODE) {
    0 { Done "Done. The FM-1 restarted into PING OS-1: you should see the boot animation." Green }
    1 { Done "Cancelled, or it stopped with an error (see above). If nothing was written, the FM-1 is as it was." Yellow }
    3 { Done "The FM-1 was not found. Check the cable, and that nothing else is using its MIDI." Yellow }
    4 { Done "The connection was lost part-way. The FM-1 stays in update mode: open this shortcut again to finish." Yellow }
    5 { Done "The FM-1 did not answer in time. If its screen shows update mode, open this shortcut again to finish." Yellow }
    6 { Done "The FM-1 refused this package (see above). Nothing was written." Yellow }
    default { Done "The installer ended with code $LASTEXITCODE (see above)." Yellow }
}
