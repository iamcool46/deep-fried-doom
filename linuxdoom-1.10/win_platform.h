//-----------------------------------------------------------------------------
// Windows platform layer – shared handles and state
// Used by win_main, i_video_win, i_sound_win, and future DX11 renderer.
//-----------------------------------------------------------------------------

#ifndef __WIN_PLATFORM__
#define __WIN_PLATFORM__

#ifdef _WIN32

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// Main window created by win_main; used by i_video for swap chain and input.
extern HWND       g_win_main_hwnd;
extern HINSTANCE  g_win_hInstance;
extern int        g_win_nCmdShow;

// Set by win_main after creating the window. Read by I_InitGraphics.
void Win_SetMainWindow(HWND hwnd, HINSTANCE hInstance, int nCmdShow);

// Request to exit the application (e.g. from WndProc on WM_CLOSE).
void Win_RequestQuit(void);
int  Win_QuitRequested(void);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */
#endif /* __WIN_PLATFORM__ */
