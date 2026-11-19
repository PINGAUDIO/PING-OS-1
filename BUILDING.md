# Building PING OS-1

PING OS-1 is built on [fm1-base](https://github.com/charlesvestal/fm1-base), which is here as a submodule (`base`,
PING AUDIO's fork: a few additions to the platform, listed in its history). Clone with the submodules:

    git clone --recurse-submodules https://github.com/PINGAUDIO/ping-os-1

## The firmware

fm1-base's build (`base/tools/build.py`; its notes say what it needs) uses the JieLi toolchain and, for Bluetooth, the
AC79 SDK. With those in place:

    python3 base/tools/build.py app --release 1.0

writes `app/build/ping-os-1-1.0.fwsc`, the package the installer page and M-VAVE's own updater take. On Windows,
`dev\fwbuild.ps1 --release 1.0` runs the same build in Docker, with the toolchain in `%USERPROFILE%\.jieli`.

The factory project (`app/factory.h`) comes from `assets/factory/default.pingos` through `tools/make_factory.py` (`--pattern-1`), and
the start-up sound (`app/sound_logo.h`) from `assets/ping_sound_logo.wav` through `tools/make_sound_logo.py`.

## On a PC

- `sim\build.ps1` builds the Windows simulator (MSVC): the same app and engine with the panel in a window.
  `PING-OS-1-sim.exe --script FILE` runs it headless for screenshots and sound files.
- `sim\web\build.ps1` builds the browser version (Emscripten in Docker) into `app/web/play`.
- `bench\build.ps1` builds the engine's bench, which renders every model and effect to files.

## The page and the manual

- `python docs/build_manual.py` writes `README.md`, `docs/manual/index.html` and the PDF from one text.
- `python tools/make_installer.py` writes the site (`dev/out/installer`): play, install, samples, manual, terms.

## Layout

    engine/   the sound engine (chisel.h is the map)
    app/      the firmware's app: screens, sequencer, storage; app/web the pages
    sim/      the platform for Windows and for the browser
    docs/     the manual's source and pictures, and THIRD_PARTY.md
    tools/    generators for tables, the factory project, the site

Licence: MPL-2.0 (LICENSE). Third-party code and its notices: docs/THIRD_PARTY.md.
