# PING OS-1: opens fm1-base's installer page for this PC only (desktop shortcut "PING OS-1 - Installer").
# A tiny web server on 127.0.0.1 (this PC alone; nothing is published) serves dev/out/installer, and Chrome or Edge
# opens it: Web MIDI needs a page from http://localhost, not a file. Close this window when you are done.
$root = Split-Path -Parent $PSScriptRoot
$Host.UI.RawUI.WindowTitle = "PING OS-1 - Installer (keep open while installing)"
$site = "$root\dev\out\installer"
$port = 8765
$py = Join-Path $env:LOCALAPPDATA 'Programs\Python\Python310\python.exe'
if (!(Test-Path -LiteralPath $py)) { $py = (Get-Command python -ErrorAction SilentlyContinue).Source }
Write-Host ""
Write-Host "  PING OS-1 - installer page (on this PC only)" -ForegroundColor Cyan
Write-Host "  ------------------------------------------"
if (!(Test-Path -LiteralPath "$site\install\index.html")) {
    Write-Host "  The installer page is missing. Build it first: devwbuild.ps1 --release 1.0" -ForegroundColor Yellow
    Read-Host "  Press Enter to close"; exit
}
$srv = Start-Process -FilePath $py -ArgumentList @('-m', 'http.server', "$port", '--bind', '127.0.0.1', '--directory', "`"$site`"") -WindowStyle Hidden -PassThru
Start-Sleep -Milliseconds 800
$url = "http://localhost:$port/"
$chrome = @("$env:ProgramFiles\Google\Chrome\Application\chrome.exe", "${env:ProgramFiles(x86)}\Google\Chrome\Application\chrome.exe",
            "$env:LOCALAPPDATA\Google\Chrome\Application\chrome.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
$edge = @("${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe", "$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe") |
        Where-Object { Test-Path $_ } | Select-Object -First 1
$browser = if ($chrome) { $chrome } elseif ($edge) { $edge } else { $null }
if ($browser) { Start-Process -FilePath $browser -ArgumentList $url } else { Start-Process $url }
Write-Host "  The installer opened in your browser: $url"
Write-Host ""
Write-Host "  In the page:"
Write-Host "   1. Plug in the FM-1, switch it on, turn its volume down."
Write-Host "   2. Press Install. When the browser asks to use MIDI devices, choose Allow."
Write-Host "   3. Wait for it to finish; keep the cable in. The FM-1 restarts into PING OS-1."
Write-Host ""
Write-Host "  Keep this window open while installing. Close it (or press Enter) when you are done." -ForegroundColor Yellow
Read-Host "  Press Enter to stop the page"
if ($srv -and !$srv.HasExited) { Stop-Process -Id $srv.Id -Force }
