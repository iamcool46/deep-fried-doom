//-----------------------------------------------------------------------------
// WAD Selection Menu - IWAD/PWAD selection before game starts
//-----------------------------------------------------------------------------

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>

#include "doomdef.h"
#include "d_main.h"
#include "win_platform.h"
#include "menu_wad.h"

#define MAX_IWADS 16
#define MAX_PWADS 32
#define MAX_PATH 512

typedef struct {
    char path[MAX_PATH];
    char name[32];
    int gamemode;  /* shareware, registered, commercial, etc. */
} iwad_t;

static iwad_t s_iwads[MAX_IWADS];
static int s_num_iwads = 0;
static int s_selected_iwad = -1;

static char s_pwads[MAX_PWADS][MAX_PATH];
static int s_num_pwads = 0;

static HWND s_hwndMenu;
static HWND s_hwndIWADList;
static HWND s_hwndPWADList;
static HWND s_hwndAddPWAD;
static HWND s_hwndRemovePWAD;
static HWND s_hwndRunGame;
static HWND s_hwndQuit;
static int s_menu_result = 0;  /* 0=quit, 1=run */
static int s_menu_done = 0;

static const char *s_iwad_names[] = {
    "doom1.wad", "doom.wad", "doomu.wad", "doom2.wad",
    "plutonia.wad", "tnt.wad", "doom2f.wad"
};

static int GetGamemodeFromIWAD(const char *name)
{
    if (strstr(name, "doom1.wad")) return 0; /* shareware */
    if (strstr(name, "doom.wad")) return 1;  /* registered */
    if (strstr(name, "doomu.wad")) return 3; /* retail */
    if (strstr(name, "doom2.wad")) return 2; /* commercial */
    if (strstr(name, "plutonia.wad")) return 2; /* commercial */
    if (strstr(name, "tnt.wad")) return 2; /* commercial */
    if (strstr(name, "doom2f.wad")) return 2; /* commercial */
    return -1;
}

static void ScanDirectoryForIWADs(const char *dir)
{
    char path[MAX_PATH];
    char fullpath[MAX_PATH];
    int i;

    /* Convert to absolute path */
    if (GetFullPathNameA(dir, MAX_PATH, fullpath, NULL) == 0)
        strncpy(fullpath, dir, MAX_PATH - 1);
    fullpath[MAX_PATH - 1] = '\0';

    for (i = 0; i < sizeof(s_iwad_names) / sizeof(s_iwad_names[0]); i++)
    {
        sprintf(path, "%s\\%s", fullpath, s_iwad_names[i]);
        if (_access(path, 4) == 0)  /* R_OK = 4 */
        {
            if (s_num_iwads < MAX_IWADS)
            {
                /* Store absolute path */
                if (GetFullPathNameA(path, MAX_PATH, s_iwads[s_num_iwads].path, NULL) == 0)
                    strncpy(s_iwads[s_num_iwads].path, path, MAX_PATH - 1);
                s_iwads[s_num_iwads].path[MAX_PATH - 1] = '\0';
                strncpy(s_iwads[s_num_iwads].name, s_iwad_names[i], 31);
                s_iwads[s_num_iwads].name[31] = '\0';
                s_iwads[s_num_iwads].gamemode = GetGamemodeFromIWAD(s_iwad_names[i]);
                s_num_iwads++;
            }
        }
    }
}

static void ScanForIWADs(void)
{
    char exedir[MAX_PATH];
    char *p;

    s_num_iwads = 0;

    /* Scan current directory */
    ScanDirectoryForIWADs(".");

    /* Scan C:\DOOM */
    ScanDirectoryForIWADs("C:\\DOOM");

    /* Scan executable directory */
    if (GetModuleFileNameA(NULL, exedir, MAX_PATH))
    {
        for (p = exedir + strlen(exedir); p > exedir && p[-1] != '\\' && p[-1] != '/'; p--)
            ;
        *p = '\0';
        if (exedir[0])
            ScanDirectoryForIWADs(exedir);
    }

    /* Default to first IWAD if found */
    if (s_num_iwads > 0)
        s_selected_iwad = 0;
}

static LRESULT CALLBACK MenuWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_COMMAND:
        {
            if ((HWND)lParam == s_hwndIWADList && HIWORD(wParam) == LBN_SELCHANGE)
            {
                s_selected_iwad = SendMessageA(s_hwndIWADList, LB_GETCURSEL, 0, 0);
                EnableWindow(s_hwndRunGame, s_selected_iwad >= 0);
            }
            else if ((HWND)lParam == s_hwndAddPWAD)
            {
                OPENFILENAMEA ofn;
                char filename[MAX_PATH] = "";
                char fullpath[MAX_PATH];
                memset(&ofn, 0, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd;
                ofn.lpstrFilter = "WAD Files\0*.wad\0All Files\0*.*\0";
                ofn.lpstrFile = filename;
                ofn.nMaxFile = MAX_PATH;
                /* OFN_FULLPATHNAME is not defined in modern SDKs; use the standard flags. */
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                if (GetOpenFileNameA(&ofn))
                {
                    if (s_num_pwads < MAX_PWADS)
                    {
                        /* Convert to absolute path */
                        if (GetFullPathNameA(filename, MAX_PATH, fullpath, NULL) == 0)
                            strncpy(fullpath, filename, MAX_PATH - 1);
                        fullpath[MAX_PATH - 1] = '\0';
                        strncpy(s_pwads[s_num_pwads], fullpath, MAX_PATH - 1);
                        s_pwads[s_num_pwads][MAX_PATH - 1] = '\0';
                        SendMessageA(s_hwndPWADList, LB_ADDSTRING, 0, (LPARAM)filename);
                        s_num_pwads++;
                    }
                }
            }
            else if ((HWND)lParam == s_hwndRemovePWAD)
            {
                int sel = SendMessageA(s_hwndPWADList, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < s_num_pwads)
                {
                    SendMessageA(s_hwndPWADList, LB_DELETESTRING, sel, 0);
                    memmove(&s_pwads[sel], &s_pwads[sel + 1], (s_num_pwads - sel - 1) * MAX_PATH);
                    s_num_pwads--;
                }
            }
            else if ((HWND)lParam == s_hwndRunGame)
            {
                if (s_selected_iwad >= 0)
                {
                    s_menu_result = 1;
                    PostMessageA(hwnd, WM_CLOSE, 0, 0);
                }
            }
            else if ((HWND)lParam == s_hwndQuit)
            {
                s_menu_result = 0;
                PostMessageA(hwnd, WM_CLOSE, 0, 0);
            }
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            /* Do NOT post WM_QUIT: that would terminate the whole app message queue. */
            s_menu_done = 1;
            return 0;
        default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
}

int Menu_RunWADSelection(void)
{
    WNDCLASSEXA wc;
    MSG msg;
    int i;

    s_menu_result = 0;
    s_menu_done = 0;
    s_num_pwads = 0;
    s_selected_iwad = -1;

    ScanForIWADs();

    if (s_num_iwads == 0)
    {
        MessageBoxA(NULL, "No IWAD files found!\n\nPlease place doom.wad, doom2.wad, or another IWAD in:\n- Current directory\n- C:\\DOOM\n- Executable directory", "DOOM - No IWAD", MB_OK | MB_ICONERROR);
        return 0;
    }

    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MenuWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = g_win_hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "DoomWADMenu";
    wc.hIconSm = NULL;
    RegisterClassExA(&wc);

    s_hwndMenu = CreateWindowExA(0, "DoomWADMenu", "DOOM - Select WAD Files",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 500,
        NULL, NULL, g_win_hInstance, NULL);

    if (!s_hwndMenu)
        return 0;

    CreateWindowA("STATIC", "IWAD (Required):", WS_VISIBLE | WS_CHILD,
        10, 10, 200, 20, s_hwndMenu, NULL, g_win_hInstance, NULL);

    s_hwndIWADList = CreateWindowA("LISTBOX", "", WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
        10, 35, 560, 120, s_hwndMenu, NULL, g_win_hInstance, NULL);

    for (i = 0; i < s_num_iwads; i++)
        SendMessageA(s_hwndIWADList, LB_ADDSTRING, 0, (LPARAM)s_iwads[i].name);

    if (s_selected_iwad >= 0)
        SendMessageA(s_hwndIWADList, LB_SETCURSEL, s_selected_iwad, 0);

    CreateWindowA("STATIC", "PWADs (Optional):", WS_VISIBLE | WS_CHILD,
        10, 165, 200, 20, s_hwndMenu, NULL, g_win_hInstance, NULL);

    s_hwndPWADList = CreateWindowA("LISTBOX", "", WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
        10, 190, 450, 200, s_hwndMenu, NULL, g_win_hInstance, NULL);

    s_hwndAddPWAD = CreateWindowA("BUTTON", "Add PWAD...", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        470, 190, 100, 30, s_hwndMenu, NULL, g_win_hInstance, NULL);

    s_hwndRemovePWAD = CreateWindowA("BUTTON", "Remove", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        470, 230, 100, 30, s_hwndMenu, NULL, g_win_hInstance, NULL);

    s_hwndRunGame = CreateWindowA("BUTTON", "Run Game", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        10, 400, 150, 40, s_hwndMenu, NULL, g_win_hInstance, NULL);

    s_hwndQuit = CreateWindowA("BUTTON", "Quit", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        440, 400, 150, 40, s_hwndMenu, NULL, g_win_hInstance, NULL);

    EnableWindow(s_hwndRunGame, s_selected_iwad >= 0);

    ShowWindow(s_hwndMenu, SW_SHOW);
    UpdateWindow(s_hwndMenu);

    while (!s_menu_done && GetMessageA(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    if (s_menu_result && s_selected_iwad >= 0)
    {
        char msg[512];
        /* Clear existing wadfiles */
        for (i = 0; i < MAXWADFILES; i++)
            wadfiles[i] = NULL;

        /* Verify IWAD file exists before adding */
        if (_access(s_iwads[s_selected_iwad].path, 4) != 0)
        {
            sprintf(msg, "IWAD file not found:\n%s\n\nPlease check the file path.", s_iwads[s_selected_iwad].path);
            MessageBoxA(NULL, msg, "DOOM - File Error", MB_OK | MB_ICONERROR);
            return 0;
        }

        /* Add selected IWAD */
        D_AddFile(s_iwads[s_selected_iwad].path);

        /* Add PWADs */
        for (i = 0; i < s_num_pwads; i++)
        {
            if (_access(s_pwads[i], 4) == 0)
                D_AddFile(s_pwads[i]);
            else
            {
                sprintf(msg, "PWAD file not found:\n%s\n\nSkipping this file.", s_pwads[i]);
                MessageBoxA(NULL, msg, "DOOM - Warning", MB_OK | MB_ICONWARNING);
            }
        }
    }

    return s_menu_result;
}

#endif /* _WIN32 */
