//-----------------------------------------------------------------------------
// Windows implementation of i_video.h
// Win32 window, GDI blit of 320x200 screen buffer (software path).
// NOW WITH: WASD controls, Mouse lock, E to use, Scroll wheel weapon change
//-----------------------------------------------------------------------------

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "doomdef.h"
#include "doomstat.h"
#include "i_system.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_main.h"
#include "win_platform.h"
#include "d_event.h"
#include "d_main.h"
#include "i_video.h"

extern byte* screens[5];
extern byte gammatable[5][256];
extern int usegamma;

static HDC s_hdcWindow;
static HDC s_hdcBitmap;
static HBITMAP s_hBitmap;
static HBITMAP s_hBitmapOld;
static int s_width, s_height;   /* window client size */
static byte s_palette[256 * 3]; /* current RGB palette */
static int s_palette_dirty = 1;
static int s_mouse_locked = 1;  /* Mouse lock enabled by default */
static POINT s_last_mouse_pos;  /* For calculating mouse delta */
static int s_mouse_initialized = 0;

static int VK_to_DoomKey(WPARAM vk)
{
    switch (vk)
    {
        /* Arrow keys */
    case VK_LEFT:   return 0xac;  /* KEY_LEFTARROW */
    case VK_RIGHT:  return 0xae;
    case VK_UP:     return 0xad;
    case VK_DOWN:   return 0xaf;

        /* WASD controls - map to arrow keys for movement */
    case 'W':       return 0xad;  /* W = Forward (UP) */
    case 'S':       return 0xaf;  /* S = Back (DOWN) */
    case 'A':       return 0xac;  /* A = Turn left (LEFT) */
    case 'D':       return 0xae;  /* D = Turn right (RIGHT) */

        /* E for use/interact */
    case 'E':       return ' ';   /* E = Use (SPACE) */

        /* Standard keys */
    case VK_ESCAPE: return 27;
    case VK_RETURN: return 13;
    case VK_TAB:    return 9;
    case VK_SPACE:  return ' ';

        /* Function keys */
    case VK_F1:     return 0x80 + 0x3b;
    case VK_F2:     return 0x80 + 0x3c;
    case VK_F3:     return 0x80 + 0x3d;
    case VK_F4:     return 0x80 + 0x3e;
    case VK_F5:     return 0x80 + 0x3f;
    case VK_F6:     return 0x80 + 0x40;
    case VK_F7:     return 0x80 + 0x41;
    case VK_F8:     return 0x80 + 0x42;
    case VK_F9:     return 0x80 + 0x43;
    case VK_F10:    return 0x80 + 0x44;
    case VK_F11:    return 0x80 + 0x57;
    case VK_F12:    return 0x80 + 0x58;

    case VK_BACK:   return 127;
    case VK_PAUSE:  return 0xff;
    case VK_ADD:
    case VK_OEM_PLUS:  return 0x3d;
    case VK_SUBTRACT:
    case VK_OEM_MINUS: return 0x2d;
    case VK_SHIFT:   return 0x80 + 0x36;
    case VK_CONTROL: return 0x80 + 0x1d;  /* Ctrl = Run */
    case VK_MENU:    return 0x80 + 0x38;  /* Alt = Strafe */
    default:
        if (vk >= ' ' && vk <= 'Z') return (int)vk;
        return 0;
    }
}

void I_ShutdownGraphics(void)
{
    if (s_hdcBitmap && s_hBitmapOld)
    {
        SelectObject(s_hdcBitmap, s_hBitmapOld);
        s_hBitmapOld = NULL;
    }
    if (s_hBitmap)
    {
        DeleteObject(s_hBitmap);
        s_hBitmap = NULL;
    }
    if (s_hdcBitmap)
    {
        DeleteDC(s_hdcBitmap);
        s_hdcBitmap = NULL;
    }
    if (s_hdcWindow)
    {
        ReleaseDC(g_win_main_hwnd, s_hdcWindow);
        s_hdcWindow = NULL;
    }

    /* Release mouse cursor */
    ShowCursor(TRUE);
}

void I_SetPalette(byte* palette)
{
    int i;
    byte* dst = s_palette;
    const byte* gamma = gammatable[usegamma];

    for (i = 0; i < 256; i++)
    {
        *dst++ = gamma[*palette++];
        *dst++ = gamma[*palette++];
        *dst++ = gamma[*palette++];
    }
    s_palette_dirty = 1;
}

void I_UpdateNoBlit(void)
{
}

void I_FinishUpdate(void)
{
    BITMAPINFO bmi;
    byte* src = screens[0];
    int x, y;
    RECT rc;
    static unsigned char* dib_bits = NULL;
    static int dib_size = 0;
    int row_bytes;

    if (!g_win_main_hwnd || !src)
        return;

    if (!s_hdcWindow)
    {
        s_hdcWindow = GetDC(g_win_main_hwnd);
        GetClientRect(g_win_main_hwnd, &rc);
        s_width = rc.right - rc.left;
        s_height = rc.bottom - rc.top;
    }

    if (!s_hdcBitmap || s_width < 1 || s_height < 1)
        return;

    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = SCREENWIDTH;
    bmi.bmiHeader.biHeight = -SCREENHEIGHT; /* top-down */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    row_bytes = SCREENWIDTH * 4;
    if (dib_size < row_bytes * SCREENHEIGHT)
    {
        if (dib_bits) free(dib_bits);
        dib_size = row_bytes * SCREENHEIGHT;
        dib_bits = (unsigned char*)malloc((size_t)dib_size);
    }
    if (!dib_bits) return;

    /* Convert 8-bit to 32-bit using current palette */
    {
        byte* pal = s_palette;
        unsigned char* out = dib_bits;
        for (y = 0; y < SCREENHEIGHT; y++)
        {
            for (x = 0; x < SCREENWIDTH; x++)
            {
                int idx = *src++;
                out[0] = pal[idx * 3 + 2];
                out[1] = pal[idx * 3 + 1];
                out[2] = pal[idx * 3 + 0];
                out[3] = 255;
                out += 4;
            }
        }
    }

    SetDIBitsToDevice(s_hdcBitmap, 0, 0, SCREENWIDTH, SCREENHEIGHT,
        0, 0, 0, SCREENHEIGHT, dib_bits, &bmi, DIB_RGB_COLORS);

    GetClientRect(g_win_main_hwnd, &rc);
    s_width = rc.right - rc.left;
    s_height = rc.bottom - rc.top;
    StretchBlt(s_hdcWindow, 0, 0, s_width, s_height,
        s_hdcBitmap, 0, 0, SCREENWIDTH, SCREENHEIGHT, SRCCOPY);
}


void I_ReadScreen(byte* scr)
{
    if (screens[0] && scr)
        memcpy(scr, screens[0], (size_t)(SCREENWIDTH * SCREENHEIGHT));
}


void I_StartFrame(void)
{
}

void I_StartTic(void)
{
    MSG msg;
    RECT rc;
    POINT center, current_pos;

    if (!g_win_main_hwnd)
        return;

    /* Calculate window center for mouse lock */
    if (s_mouse_locked)
    {
        GetClientRect(g_win_main_hwnd, &rc);
        center.x = rc.right / 2;
        center.y = rc.bottom / 2;
        ClientToScreen(g_win_main_hwnd, &center);

        if (!s_mouse_initialized)
        {
            SetCursorPos(center.x, center.y);
            s_last_mouse_pos = center;
            s_mouse_initialized = 1;
            ShowCursor(FALSE);  /* Hide cursor when mouse locked */
        }
    }

    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            Win_RequestQuit();
            break;
        }

        /* Keyboard input */
        if (msg.message == WM_KEYDOWN || msg.message == WM_KEYUP)
        {
            int key = VK_to_DoomKey(msg.wParam);
            if (key)
            {
                event_t ev;
                ev.type = (msg.message == WM_KEYDOWN) ? ev_keydown : ev_keyup;
                ev.data1 = key;
                ev.data2 = ev.data3 = 0;
                D_PostEvent(&ev);
            }
        }

        /* Mouse wheel for weapon switching */
        else if (msg.message == WM_MOUSEWHEEL)
        {
            short delta = GET_WHEEL_DELTA_WPARAM(msg.wParam);
            event_t ev;
            ev.type = ev_keydown;

            if (delta > 0)
            {
                /* Scroll up = previous weapon */
                ev.data1 = 0x2d;  /* - key */
            }
            else
            {
                /* Scroll down = next weapon */
                ev.data1 = 0x3d;  /* = key */
            }
            ev.data2 = ev.data3 = 0;
            D_PostEvent(&ev);
        }

        /* Mouse movement - with lock support */
        else if (msg.message == WM_MOUSEMOVE)
        {
            if (s_mouse_locked)
            {
                GetCursorPos(&current_pos);

                /* Calculate delta from last position */
                int dx = current_pos.x - s_last_mouse_pos.x;
                int dy = current_pos.y - s_last_mouse_pos.y;

                if (dx != 0 || dy != 0)
                {
                    event_t ev;
                    ev.type = ev_mouse;
                    ev.data1 = 0;  /* No buttons in this event */
                    ev.data2 = dx * 8;  /* Multiply for sensitivity */
                    ev.data3 = -dy * 8; /* Negative because screen Y is inverted */
                    D_PostEvent(&ev);

                    /* Re-center cursor */
                    SetCursorPos(center.x, center.y);
                    s_last_mouse_pos = center;
                }
            }
        }

        /* Mouse buttons */
        else if (msg.message == WM_LBUTTONDOWN || msg.message == WM_LBUTTONUP ||
            msg.message == WM_RBUTTONDOWN || msg.message == WM_RBUTTONUP ||
            msg.message == WM_MBUTTONDOWN || msg.message == WM_MBUTTONUP)
        {
            event_t ev;
            ev.type = ev_mouse;
            ev.data1 = (msg.wParam & MK_LBUTTON ? 1 : 0) |
                (msg.wParam & MK_RBUTTON ? 2 : 0) |
                (msg.wParam & MK_MBUTTON ? 4 : 0);
            ev.data2 = 0;  /* No movement in button events */
            ev.data3 = 0;
            D_PostEvent(&ev);
        }

        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

void I_InitGraphics(void)
{
    RECT rc;
    BITMAPINFO bmi;

    if (!g_win_main_hwnd)
        I_Error("I_InitGraphics: no window");

    s_hdcWindow = GetDC(g_win_main_hwnd);
    GetClientRect(g_win_main_hwnd, &rc);
    s_width = rc.right - rc.left;
    s_height = rc.bottom - rc.top;
    if (s_width < 320) s_width = 320;
    if (s_height < 200) s_height = 200;

    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = SCREENWIDTH;
    bmi.bmiHeader.biHeight = -SCREENHEIGHT;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    s_hBitmap = CreateCompatibleBitmap(s_hdcWindow, SCREENWIDTH, SCREENHEIGHT);
    s_hdcBitmap = CreateCompatibleDC(s_hdcWindow);
    if (s_hdcBitmap && s_hBitmap)
        s_hBitmapOld = (HBITMAP)SelectObject(s_hdcBitmap, s_hBitmap);

    /* Default palette (will be replaced by I_SetPalette when game loads) */
    {
        int i;
        memset(s_palette, 0, sizeof(s_palette));
        for (i = 0; i < 256; i++)
            s_palette[i * 3 + 0] = s_palette[i * 3 + 1] = s_palette[i * 3 + 2] = (byte)i;
    }
    s_palette_dirty = 1;

    /* Enable mouse lock */
    s_mouse_locked = 1;
    s_mouse_initialized = 0;
    ShowCursor(FALSE);

    printf("Controls enabled:\n");
    printf("  WASD - Movement\n");
    printf("  E - Use/Interact\n");
    printf("  Mouse - Look/Aim\n");
    printf("  Scroll Wheel - Change Weapons\n");
    printf("  Left Click - Fire\n");
}

#endif /* _WIN32 */