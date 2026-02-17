# DOOM Windows 11 Port – Architecture & Migration Plan

This document describes the analysis, migration strategy, and module structure for porting the original id Software Linux Doom 1.10 source to **Windows 11** with a **native DirectX 11** true 3D renderer and a **modern start menu**, while preserving vanilla gameplay, WAD compatibility, and demo determinism.

---

## 1. Codebase Analysis

### 1.1 Repository Structure

- **`linuxdoom-1.10/`** – Core game engine (id Linux Doom 1.10).
- **`sndserv/`** – Optional Linux sound server (not used in default build).
- **`sersrc/`**, **`ipx/`** – Serial/IPX networking (optional).

### 1.2 Engine Layers (Current)

| Layer | Files | Role |
|-------|--------|------|
| **Entry** | `i_main.c` | `main()` → `D_DoomMain()` |
| **System** | `i_system.c/h` | Time (`gettimeofday`), zone base, quit, error, `I_StartFrame`/`I_StartTic`, `I_BaseTiccmd` |
| **Video** | `i_video.c/h` | X11 display, window, SHM image, events, palette, `I_InitGraphics`, `I_FinishUpdate` |
| **Sound** | `i_sound.c/h` | Linux OSS / sound server; `I_InitSound`, `I_StartSound`, `I_UpdateSound`, music |
| **Net** | `i_net.c/h` | Network (Linux sockets) |
| **Game loop** | `d_main.c` | `D_DoomMain()`: IdentifyVersion, W_Init, R_Init, P_Init, I_Init, S_Init, then `D_DoomLoop()` |
| **Display** | `d_main.c` | `D_Display()`: wipe, then `R_RenderPlayerView()`, HUD, menu, `I_FinishUpdate()` |
| **Renderer** | `r_*.c/h` | BSP (`r_bsp`), segs (`r_segs`), planes (`r_plane`), draw (`r_draw`), things (`r_things`), data, main, sky |
| **Video buffer** | `v_video.c/h` | `screens[]`, `V_DrawPatch`, `V_DrawBlock`, gamma |
| **WAD** | `w_wad.c/h` | `W_InitMultipleFiles`, `W_CacheLumpName`, lump I/O – **unchanged** |
| **Game logic** | `p_*.c`, `g_game.c`, etc. | Physics, mobj, map, sector logic – **unchanged** |

### 1.3 Linux / POSIX Dependencies to Remove

- **Video:** X11 (`Xlib`, `Xutil`, `XShm`), `sys/ipc.h`, `sys/shm.h`.
- **System:** `sys/time.h` (`gettimeofday`), `unistd.h` (`usleep`), `signal.h`, `malloc`/exit (replace with Windows equivalents where required).
- **Sound:** `linux/soundcard.h`, OSS ioctl, or sndserv process.
- **Net:** Unix sockets (optional to replace with Winsock later).
- **Build:** gcc, Makefile, `-lX11 -lXext -lnsl -lm`.

### 1.4 What Must Be Preserved (No Rewrite)

- **WAD:** `w_wad.c/h`, lump names, `W_InitMultipleFiles`, `W_CacheLump*` – **do not replace**.
- **Game logic:** All `p_*.c` (map, mobj, doors, floor, ceiling, etc.), `g_game.c`, `d_net.c` – **unchanged**.
- **BSP / map data:** `r_bsp.c` traversal, `nodes[]`, `subsectors[]`, `segs[]`, `sectors[]`, `line_t`, `side_t` – **keep**; only the *drawing* is replaced.
- **Demo / tic flow:** `D_DoomLoop`, `TryRunTics`, `G_Ticker`, `I_GetTime` semantics (e.g. 35 Hz logic) – **preserved**.
- **Rendering interface used by engine:** The engine calls `R_RenderPlayerView()` and expects `I_FinishUpdate()`. We will keep the same call sites and replace *implementation* (software column → DX11 3D).

---

## 2. Migration Strategy: Linux → Windows 11

### 2.1 Platform Layer Replacement

| Component | Remove | Replace With |
|-----------|--------|--------------|
| **Entry** | `main()` in `i_main.c` | `WinMain()` or `main()` in new `win_main.c` (MSVC), calling into same `D_DoomMain()` after optional menu. |
| **Time** | `gettimeofday` | `QueryPerformanceCounter` / `QueryPerformanceFrequency`. |
| **Windowing** | X11 Display, Window, XShm | Win32: `CreateWindowEx`, `WndProc`, `GetMessage`/`PeekMessage`, no XShm. |
| **Graphics buffer** | XImage / SHM buffer | Either (a) stub: still provide `screens[0]` for legacy code path until DX11 is active, or (b) DX11 back buffer as “logical” screen. |
| **Input** | XNextEvent, XKeycodeToKeysym, XWarpPointer | Win32 `WM_KEYDOWN`/`WM_LBUTTONDOWN`/etc., or Raw Input; map to same `event_t` and `D_PostEvent()`. |
| **Sound** | OSS / sndserv | XAudio2 or WASAPI (prefer XAudio2 for simplicity). |
| **Threading** | (minimal in original) | Windows: `CreateThread`, `_beginthreadex`, or keep single-threaded where possible. |
| **Config path** | `$HOME/.doomrc` | `%APPDATA%\DOOM\default.cfg` or under exe dir. |
| **IWAD discovery** | `getenv("DOOMWADDIR")`, `access(doomwad, R_OK)` | Scan: current dir, `C:\DOOM`, exe dir; use `_access` or `GetFileAttributes`. |

### 2.2 Build System

- **Target:** Visual Studio 2022, MSVC, native Windows only (no MinGW, no Cygwin).
- **Artifacts:** One solution (e.g. `DOOM.sln`), one or more projects (e.g. `doom.vcxproj`).
- **Source:** All `linuxdoom-1.10/*.c` (except replaced `i_*.c` with `win_*` or `i_*_win.c`), plus new `win_*.c`, `dx11_*.c`, `menu_*.c`.
- **Defines:** Remove `NORMALUNIX`, `LINUX`; add `_WIN32`, `WIN32`, optionally `_CRT_SECURE_NO_WARNINGS`.
- **Libraries:** No X11; link `user32.lib`, `kernel32.lib`, `d3d11.lib`, `dxgi.lib`, `xaudio2.lib` (or equivalent).

### 2.3 File-Level Modification Map

- **Replace entirely (new implementation):**
  - `i_main.c` → `win_main.c` (or keep `i_main.c` with `#ifdef _WIN32` and WinMain in separate file).
  - `i_system.c` → Windows time, zone base, quit, error.
  - `i_video.c` → Win32 window + (eventually) DX11 swap chain; or split: `i_video_win.c` (window) + `r_dx11.c` (render).
  - `i_sound.c` → XAudio2/WASAPI implementation.
- **Modify (minimal, for Windows paths and startup):**
  - `d_main.c`: `IdentifyVersion()` – IWAD paths for Windows; optionally *do not* call `D_DoomMain()` directly from `main()` if we show menu first (see Start Menu below).
- **Leave unchanged at first:** All `r_*.c` except we add a *new* DX11 render path that the engine will call instead of the software column drawer (see Renderer section).
- **Optional later:** `i_net.c` for Winsock.

---

## 3. Renderer Replacement Strategy (Software Column → DirectX 11 True 3D)

### 3.1 Current Software Pipeline (Keep Logic, Replace Drawing)

1. **`R_RenderPlayerView(player)`** (r_main.c):
   - `R_SetupFrame(player)` – set `viewx`, `viewy`, `viewz`, `viewangle`, etc.
   - Clear clip/draw segs, planes, sprites.
   - **`R_RenderBSPNode(numnodes-1)`** – BSP traversal; for each subsector calls **`R_Subsector()`**.
2. **`R_Subsector()`** (r_segs.c): adds segs to draw list; column-based wall drawing.
3. **`R_DrawPlanes()`** (r_plane.c): visplane-based floors/ceilings.
4. **`R_DrawMasked()`** (r_things.c): sprites, mid-textures.

We **keep**: `R_SetupFrame`, BSP traversal (`R_RenderBSPNode`), and the *data* (nodes, subsectors, segs, sectors, linedefs, sides).  
We **replace**: the actual pixel/column drawing with a **DX11 3D path** that uses the same view and map data.

### 3.2 New DX11 Renderer Design

- **API:** DirectX 11 only (no OpenGL, no SDL).
- **Responsibilities:**
  - Build 3D meshes from current map: sectors → floor/ceiling geometry; linedefs/sides → wall quads; sprites → billboard quads.
  - Maintain vertex/index buffers (or dynamic VB/IB) for walls, floors, ceilings, sprites.
  - Use BSP only for *ordering* or culling (optional: still traverse BSP to collect visible subsectors, then submit their geometry).
  - One 3D camera: position `(viewx, viewy, viewz)`, yaw from `viewangle`, pitch for free look (new).
  - Perspective projection, no column/visplane limits.
- **Features (as per spec):** True 3D camera (free look up/down), perspective-correct rendering, real 3D floors/ceilings, sector-based dynamic lighting, shadow mapping, bloom, god rays, optional normal mapping.
- **Integration point:** Either:
  - **Option A:** Replace `R_RenderPlayerView()` body with a call into `R_DX11_RenderPlayerView()` and do not call `R_RenderBSPNode`/`R_DrawPlanes`/`R_DrawMasked` in the software path when DX11 is active; or
  - **Option B:** Keep `R_RenderPlayerView()` signature; inside it, if `r_use_dx11` (or similar), call DX11 path and skip software drawing; otherwise fall back to software for compatibility.

### 3.3 True 3D Geometry Conversion

- **Sectors:** For each sector, build floor and ceiling meshes (triangulate from subsector boundaries; support slopes later).
- **Walls:** Each seg/side → quad (two triangles) with correct world positions (fixed_t → float), UVs from texture.
- **Sprites:** Billboard quads, oriented to camera; use existing sprite lump/patches.
- **Textures:** Load from WAD (lumps); upload to D3D11 textures; use for walls, floors, ceilings.
- **Lighting:** Per-sector light level → vertex or pixel light; extra lights (muzzle, torches) as additional sources (dynamic lights).
- **Z-axis:** Full `viewz` and sector floor/ceiling heights; no “2.5D” restriction.

### 3.4 Files to Add (Renderer)

- `r_dx11.h` / `r_dx11.c` – DX11 device, context, swap chain, render target view.
- `r_dx11_mesh.c` – Build sector/seg/sprite meshes from `sectors[]`, `segs[]`, `subsectors[]`, etc.
- `r_dx11_shaders` – HLSL vertex/pixel shaders (lit, textured, shadows, post-process).
- Optional: `r_dx11_light.c` for dynamic lights and shadow map pass.

### 3.5 Files to Modify (Renderer)

- **r_main.c:** In `R_RenderPlayerView()`, branch to DX11 path when enabled; keep `R_SetupFrame()` and view setup.
- **r_data.c / r_defs.h:** Expose read-only map data (sectors, segs, subsectors, nodes) to DX11 mesh builder; no change to gameplay.
- **v_video / i_video:** When DX11 is active, `I_FinishUpdate()` can present swap chain; `screens[0]` may be unused for 3D view (HUD/menu can still use a small buffer or render to back buffer via DX11).

---

## 4. Start Menu Integration Plan

### 4.1 Boot Flow Change

- **Current:** `main()` → `D_DoomMain()` → IdentifyVersion, W_Init, … → `D_DoomLoop()` (which calls `I_InitGraphics()` at start of loop).
- **New:**  
  1. **Windows entry** (`WinMain` or `main`): Initialize minimal platform (e.g. Win32 window or offscreen), then run **Start Menu**.
  2. **Start Menu:**  
     - IWAD detection (scan current dir, `C:\DOOM`, exe dir).  
     - IWAD dropdown.  
     - PWAD list + file browser + drag-and-drop.  
     - Resolution, quality, fullscreen.  
     - “Run Game” button.  
  3. On “Run Game”: set `wadfiles[]` (and optionally `gamemode`, `startepisode`, `startmap`) from menu choices, then call **`D_DoomMain()`** (or a variant that skips IWAD scan and uses pre-filled `wadfiles`).  
  4. Rest of startup and **`D_DoomLoop()`** unchanged; first frame can be title screen or E1M1 as selected.

### 4.2 IWAD / PWAD Handling

- **IdentifyVersion():** Either (a) keep and run after menu (menu only adds PWADs and maybe overrides path), or (b) in menu mode, **skip** automatic IdentifyVersion and have menu set: primary IWAD path + `gamemode` (from IWAD name: doom.wad, doom2.wad, tnt.wad, plutonia.wad).
- **D_AddFile:** Menu “Run Game” builds `wadfiles[]` and calls `D_AddFile()` for each before invoking `D_DoomMain()` or a `D_DoomMainFromMenu()` that does not call `IdentifyVersion()` and uses existing `wadfiles[]`.

### 4.3 Menu Implementation

- **Technology:** DirectX 11 immediate-mode UI or custom 2D drawing (fullscreen quad with buttons/list rendered by DX11).
- **Placement:** New module `menu_main.c/h` (or `ui_startmenu.c`); no dependency on `m_menu.c` (in-game menu); start menu is shown *before* game init.
- **Window:** Can use the same Win32 window that will later host the game; menu runs in a simple 2D/DX11 UI layer.

### 4.4 Files to Add (Menu)

- `menu_main.c/h` – IWAD scan, PWAD list, resolution/options, “Run Game” and callback into startup.
- `win_main.c` – Entry; create window; run menu; on “Run Game” call `D_DoomMain()` or `D_DoomMainFromMenu()`.

### 4.5 Files to Modify (Menu / Startup)

- **d_main.c:** Add `D_DoomMainFromMenu(char **wadfiles_override, int nwad, ...)` that skips `IdentifyVersion()` and uses provided wad list; or add a flag `start_from_menu` and fill `wadfiles[]` before `IdentifyVersion()` from menu.
- **d_main.c:** Optional: move `I_InitGraphics()` earlier if window is created at menu stage (so one window for menu + game).

---

## 5. Module / Class Structure (Layered)

```
┌─────────────────────────────────────────────────────────────────┐
│  UI Layer (Start Menu)                                          │
│  menu_main.c – IWAD/PWAD selection, options, Run Game             │
└─────────────────────────────────────────────────────────────────┘
                                  │
┌─────────────────────────────────────────────────────────────────┐
│  Platform Layer (Windows)                                        │
│  win_main.c   – WinMain, message loop                             │
│  i_system.c   – I_GetTime (QPC), I_ZoneBase, I_Quit, I_Error     │
│  i_video.c    – Win32 window, D3D11 swap chain, input → events   │
│  i_sound.c    – XAudio2/WASAPI                                   │
└─────────────────────────────────────────────────────────────────┘
                                  │
┌─────────────────────────────────────────────────────────────────┐
│  Renderer Layer                                                  │
│  r_main.c, r_bsp.c (BSP + view setup – keep)                     │
│  r_dx11.c, r_dx11_mesh.c – DX11 3D draw (replace column draw)    │
│  v_video.c   – Buffers/patches for HUD/menu (keep or adapt)       │
└─────────────────────────────────────────────────────────────────┘
                                  │
┌─────────────────────────────────────────────────────────────────┐
│  Engine Core (unchanged logic)                                   │
│  d_main.c, g_game.c, p_*.c, w_wad.c, z_zone.c, etc.              │
└─────────────────────────────────────────────────────────────────┘
                                  │
┌─────────────────────────────────────────────────────────────────┐
│  Resource Layer (unchanged)                                       │
│  w_wad.c/h – W_InitMultipleFiles, W_CacheLump*, lump I/O         │
└─────────────────────────────────────────────────────────────────┘
```

---

## 6. Implementation Order (Summary)

1. **Phase 1 – Windows platform**
   - Win32 entry (`win_main.c`), window creation, message loop.
   - `i_system.c` Windows: QPC time, zone, quit, error.
   - `i_video.c` Windows: Win32 window, stub `I_FinishUpdate` (e.g. clear to black), input → `D_PostEvent`.
   - `i_sound.c` Windows: XAudio2 (or stub) so `S_Init` doesn’t crash.
   - MSVC solution/project; build and run to “black window, no crash”.

2. **Phase 2 – Start menu**
   - IWAD scan (current dir, `C:\DOOM`, exe dir).
   - Simple menu (GDI or DX11 2D): IWAD list, PWAD add, Resolution, Fullscreen, “Run Game”.
   - On “Run Game”: set `wadfiles[]`, call `D_DoomMain()` (or `D_DoomMainFromMenu()`).

3. **Phase 3 – DX11 renderer**
   - Init D3D11 device, swap chain (in `I_InitGraphics` or separate).
   - Mesh builder: sectors → floor/ceiling; segs → walls; sprites → billboards.
   - Replace `R_RenderPlayerView()` body with DX11 submit; keep `R_SetupFrame()` and BSP for visibility/culling.
   - Basic shaders (textured, lit); then shadows, bloom, god rays.

4. **Phase 4 – Polish**
   - Config path `%APPDATA%\DOOM`, 144+ FPS target (uncap render; keep 35 Hz tic).
   - Optional: DeHackEd, multithreaded rendering.

---

## 7. Compatibility Checklist

- [ ] IWAD/PWAD loading unchanged (W_* only).
- [ ] Gameplay logic untouched (p_*, g_game, d_net).
- [ ] Demo sync: same tic timing and `I_GetTime()` behavior.
- [ ] No SDL, OpenGL, Unity, Unreal; DirectX 11 only for render.
- [ ] No rewrite of WAD parser or game rules.

This document is the single source of truth for the port’s architecture; implementation will follow these layers and this order.
