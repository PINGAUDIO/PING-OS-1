# PING OS-1: asks the FM-1 what it is running. Writes nothing. (Desktop shortcut "PING OS-1 - Check FM-1".)
$Host.UI.RawUI.WindowTitle = "PING OS-1 - Check FM-1"
$root = Split-Path -Parent $PSScriptRoot
# the Python with the MIDI libraries (mido, python-rtmidi), by its full path: "python" alone can be the Store's stand-in
$py = Join-Path $env:LOCALAPPDATA 'Programs\Python\Python310\python.exe'
if (!(Test-Path -LiteralPath $py)) { $py = (Get-Command python -ErrorAction SilentlyContinue).Source }
Write-Host ""
Write-Host "  PING OS-1 - check the FM-1 (nothing is written)" -ForegroundColor Cyan
Write-Host "  ---------------------------------------------"
Write-Host "  Looking for the FM-1 over USB ..."
Write-Host ""
$out = & $py "$root\base\tools\fm1_install.py" --info 2>&1 | Out-String
$code = $LASTEXITCODE
if ($out -match 'needs mido') {
    Write-Host "  Setup problem on this PC (not the FM-1): Python's MIDI libraries are missing for $py" -ForegroundColor Yellow
    Write-Host "  Check the setup with this message."
} elseif ($code -ne 0 -or -not $out.Trim()) {
    Write-Host "  The FM-1 was not found." -ForegroundColor Yellow
    Write-Host "  - Is it plugged in by USB and switched on?"
    Write-Host "  - Close anything else using its MIDI (a DAW, a web page, M-UPGRADE), then try again."
    if ($out.Trim()) { Write-Host ""; Write-Host "  Details: $($out.Trim())" -ForegroundColor DarkGray }
} else {
    Write-Host "  Found: $($out.Trim())" -ForegroundColor Green
    Write-Host ""
    if ($out -match 'FM-1_0\d\d') { Write-Host "  That is M-VAVE's standard firmware. PING OS-1 can be installed over it." }
    elseif ($out -match 'FM-1_888') { Write-Host "  That is PING OS-1. Installing again replaces it with the newest build." }
    elseif ($out -match 'OFFICIAL_OPEN|COMMUNITY') { Write-Host "  That is M-VAVE's FM1 Open firmware: put the standard firmware back with M-UPGRADE first." -ForegroundColor Yellow }
    else { Write-Host "  That is another firmware. The installer will say whether it can install over it." }
}
Write-Host ""
Read-Host "  Press Enter to close"
