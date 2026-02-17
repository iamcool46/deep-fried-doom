//-----------------------------------------------------------------------------
// Windows entry point and main window.
// Creates Win32 window, runs start menu (or stub), then D_DoomMain().
//-----------------------------------------------------------------------------

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>

#include "doomdef.h"
#include "m_argv.h"
#include "d_main.h"
#include "win_platform.h"
#include "menu_wad.h"

HWND       g_win_main_hwnd = NULL;
HINSTANCE  g_win_hInstance = NULL;
int        g_win_nCmdShow  = 0;
static int s_quit_requested;
static int s_run_game_requested;
static HWND     s_hwnd;
static HINSTANCE s_hInstance;
static int      s_nCmdShow;

// Forward declarations
static LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
static int RunStartMenu(void);

void Win_SetMainWindow(HWND hwnd, HINSTANCE hInstance, int nCmdShow)
{
    g_win_main_hwnd = hwnd;
    g_win_hInstance = hInstance;
    g_win_nCmdShow  = nCmdShow;
}

void Win_RequestQuit(void)
{
    s_quit_requested = 1;
}

int Win_QuitRequested(void)
{
    return s_quit_requested;
}

static int VK_to_DoomKey(WPARAM vk)
{
    switch (vk)
    {
        case VK_LEFT:   return KEY_LEFTARROW;
        case VK_RIGHT:  return KEY_RIGHTARROW;
        case VK_UP:     return KEY_UPARROW;
        case VK_DOWN:   return KEY_DOWNARROW;
        case VK_ESCAPE: return KEY_ESCAPE;
        case VK_RETURN: return KEY_ENTER;
        case VK_TAB:    return KEY_TAB;
        case VK_F1:     return KEY_F1;
        case VK_F2:     return KEY_F2;
        case VK_F3:     return KEY_F3;
        case VK_F4:     return KEY_F4;
        case VK_F5:     return KEY_F5;
        case VK_F6:     return KEY_F6;
        case VK_F7:     return KEY_F7;
        case VK_F8:     return KEY_F8;
        case VK_F9:     return KEY_F9;
        case VK_F10:    return KEY_F10;
        case VK_F11:    return KEY_F11;
        case VK_F12:    return KEY_F12;
        case VK_BACK:   return KEY_BACKSPACE;
        case VK_PAUSE:  return KEY_PAUSE;
        case VK_OEM_PLUS:
        case VK_ADD:    return KEY_EQUALS;
        case VK_OEM_MINUS:
        case VK_SUBTRACT: return KEY_MINUS;
        case VK_SHIFT:  return KEY_RSHIFT;
        case VK_CONTROL: return KEY_RCTRL;
        case VK_MENU:   return KEY_RALT;
        default:
            if (vk >= ' ' && vk <= 'z')
                return (int)vk;
            return 0;
    }
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CLOSE:
            Win_RequestQuit();
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_MOUSEMOVE:
            /* Events handled by I_StartTic after engine init */
            return 0;
        default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}

/* Run WAD selection menu before starting game */
static int RunStartMenu(void)
{
    /* Use the proper WAD selection menu */
    return Menu_RunWADSelection();
}


/* Plan (pseudocode):
   - Allocate a console.
   - Try to associate standard streams with console using freopen.
   - If freopen fails, open a temporary FILE* to the console device and duplicate its file descriptor
     onto the existing standard stream file descriptor with _dup2 (do NOT assign to the stdout/stderr
     macros, as they're macros to functions and are not lvalues).
   - Close the temporary FILE* if used.
   - Repeat for stdout, stderr, stdin.
*/

void SetupConsole()
{
#ifdef _WIN32
    AllocConsole();
    FILE* f;

    /* stdout */
    f = freopen("CONOUT$", "w", stdout);
    if (!f)
    {
        FILE* temp = fopen("CONOUT$", "w");
        if (temp)
        {
            int tempfd = _fileno(temp);
            int outfd = _fileno(stdout);
            if (tempfd != -1 && outfd != -1)
            {
                _dup2(tempfd, outfd);
            }
            fclose(temp);
        }
    }

    /* stderr */
    f = freopen("CONOUT$", "w", stderr);
    if (!f)
    {
        FILE* temp = fopen("CONOUT$", "w");
        if (temp)
        {
            int tempfd = _fileno(temp);
            int errfd = _fileno(stderr);
            if (tempfd != -1 && errfd != -1)
            {
                _dup2(tempfd, errfd);
            }
            fclose(temp);
        }
    }

    /* stdin */
    f = freopen("CONIN$", "r", stdin);
    if (!f)
    {
        FILE* temp = fopen("CONIN$", "r");
        if (temp)
        {
            int tempfd = _fileno(temp);
            int infd = _fileno(stdin);
            if (tempfd != -1 && infd != -1)
            {
                _dup2(tempfd, infd);
            }
            fclose(temp);
        }
    }
#endif
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    /* Force a console window for debug output */
    SetupConsole();
    WNDCLASSEXA wc;
    int argc = 0;
    char **argv = NULL;
    char *cmdline;
    char *p;

    (void)hPrevInstance;
    (void)lpCmdLine;

    g_win_main_hwnd = NULL;
    g_win_hInstance = hInstance;
    g_win_nCmdShow  = nCmdShow;
    s_quit_requested = 0;

    /* Build argv from GetCommandLineA for m_argv compatibility */
    cmdline = GetCommandLineA();
    if (cmdline && *cmdline)
    {
        p = cmdline;
        while (*p) { if (*p == ' ' || *p == '\t') argc++; p++; }
        argc++; /* number of arguments */
        argv = (char **)malloc((argc + 1) * sizeof(char *));
        if (argv)
        {
            argc = 0;
            p = cmdline;
            while (*p == ' ' || *p == '\t') p++;
            argv[argc++] = "doom.exe";
            while (*p)
            {
                while (*p && *p != ' ' && *p != '\t') p++;
                while (*p == ' ' || *p == '\t') p++;
                if (*p) argv[argc++] = p;
            }
            argv[argc] = NULL;
        }
    }
    if (!argv) { argc = 1; argv = (char **)malloc(2 * sizeof(char *)); argv[0] = "doom.exe"; argv[1] = NULL; }
    myargc = argc;
    myargv = argv;

    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = MainWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = "DoomWin32";
    wc.hIconSm       = NULL;
    if (!RegisterClassExA(&wc))
    {
        MessageBoxA(NULL, "RegisterClassEx failed", "DOOM", MB_OK);
        return 1;
    }

    s_hwnd = CreateWindowExA(
        0,
        "DoomWin32",
        "DOOM",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        640, 400,
        NULL, NULL, hInstance, NULL
    );
    if (!s_hwnd)
    {
        MessageBoxA(NULL, "CreateWindowEx failed", "DOOM", MB_OK);
        return 1;
    }

    s_hInstance = hInstance;
    s_nCmdShow  = nCmdShow;
    Win_SetMainWindow(s_hwnd, hInstance, nCmdShow);

    /* Start menu: WAD selection - do not load map until user chooses Run Game */
    if (!RunStartMenu())
        return 0;  /* User quit */

    if (!wadfiles[0]) {
        MessageBoxA(NULL, "No WAD file selected! Exiting.", "DOOM", MB_OK | MB_ICONERROR);
        return 1;
    }

    /* Verify WAD file exists */
    if (_access(wadfiles[0], 4) != 0) {
        char msg[512];
        sprintf(msg, "Selected WAD file not found:\n%s\n\nPlease select a valid WAD file.", wadfiles[0]);
        MessageBoxA(NULL, msg, "DOOM Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    /* Ensure main window is visible and active before starting engine */
    ShowWindow(s_hwnd, SW_SHOW);
    UpdateWindow(s_hwnd);
    SetForegroundWindow(s_hwnd);
    SetFocus(s_hwnd);
    
    /* Verify window is still valid */
    if (!IsWindow(s_hwnd)) {
        MessageBoxA(NULL, "Main window is invalid!", "DOOM Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    /* User chose Run Game: start engine (same as original main() -> D_DoomMain) */
    /* Window is already created, so I_InitGraphics will use it */
    D_DoomMain();
    return 0;
}

#endif /* _WIN32 */
