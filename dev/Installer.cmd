@echo off
rem PING OS-1: opens the installer page (dev\installer.ps1). The desktop shortcut "PING OS-1 - Installer" points here.
title PING OS-1 - Installer
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer.ps1"
