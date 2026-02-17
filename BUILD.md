# Building DOOM (Windows port)

## Requirements

- **Visual Studio 2022** (or 2019) with "Desktop development with C++"
- **Windows 10/11** SDK (usually installed with VS)

## Build steps

1. Open `DOOM.sln` in Visual Studio.
2. Select configuration: **Release** (or Debug) and **x86** or **x64**.
3. Build Solution (Ctrl+Shift+B).

Alternatively, from a **Developer Command Prompt for VS**:

```bat
cd path\to\DOOM
msbuild DOOM.sln /p:Configuration=Release /p:Platform=x86
```

Output: `bin\Win32\Release\DOOM.exe` (or `bin\x64\Release\DOOM.exe`).

## Run

1. Copy an IWAD (`doom.wad`, `doom2.wad`, etc.) into one of:
   - The same folder as `DOOM.exe`
   - `C:\DOOM`
   - The directory containing `DOOM.exe`
2. Run `DOOM.exe`.
3. On the start screen, click or press a key to start the game.

## Implemented (Phase 1)

- **Win32 entry** (`win_main.c`): `WinMain`, window class, message loop, minimal start screen.
- **Platform** (`i_system_win.c`): `I_GetTime` (QueryPerformanceCounter), zone, quit, error.
- **Video** (`i_video_win.c`): Win32 window, GDI blit of 320×200 software buffer, input → events.
- **Sound** (`i_sound_win.c`): Stubs for all `I_*` sound/music (no audio yet).
- **d_main.c**: Windows IWAD search (current dir, `C:\DOOM`, exe dir); config path `%APPDATA%\DOOM\default.cfg`.
- **doomdef.h**: `SNDSERV` disabled when `_WIN32` is defined.

The **original software column renderer** is unchanged; the game draws into `screens[0]` and the Windows layer blits it to the window.

## Not yet implemented

- **Start menu**: Full IWAD/PWAD list, resolution, options (currently only “click/key to start”).
- **DirectX 11** true 3D renderer (see `ARCHITECTURE.md`).
- **XAudio2/WASAPI** for real sound and music.

## Project layout

- **DOOM.sln** / **DOOM.vcxproj**: Solution and project (MSVC).
- **linuxdoom-1.10/**: Engine sources; Windows-specific files:
  - `win_main.c`, `win_platform.h`
  - `i_system_win.c`, `i_video_win.c`, `i_sound_win.c`
- **ARCHITECTURE.md**: Full migration and renderer plan.

The build **excludes** the Linux-only modules: `i_main.c`, `i_system.c`, `i_video.c`, `i_sound.c`.
