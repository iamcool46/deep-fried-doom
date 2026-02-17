//-----------------------------------------------------------------------------
// Windows implementation of i_system.h
// Time: QueryPerformanceCounter; Zone: malloc; Quit/Error: exit.
//-----------------------------------------------------------------------------

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "doomdef.h"
#include "m_misc.h"
#include "i_video.h"
#include "i_sound.h"
#include "d_main.h"
#include "d_net.h"
#include "g_game.h"
#include "i_system.h"

int mb_used = 6;

static LARGE_INTEGER s_perf_freq;
static LARGE_INTEGER s_perf_base;
static int s_time_inited;

void I_Tactile(int on, int off, int total)
{
    (void)on;
    (void)off;
    (void)total;
}

static ticcmd_t emptycmd;

ticcmd_t *I_BaseTiccmd(void)
{
    return &emptycmd;
}

int I_GetHeapSize(void)
{
    return mb_used * 1024 * 1024;
}

byte *I_ZoneBase(int *size)
{
    *size = mb_used * 1024 * 1024;
    return (byte *)malloc((size_t)*size);
}

/* Returns time in 1/35 second tics (TICRATE). */
int I_GetTime(void)
{
    LARGE_INTEGER now;
    __int64 elapsed;

    if (!s_time_inited)
    {
        QueryPerformanceFrequency(&s_perf_freq);
        QueryPerformanceCounter(&s_perf_base);
        s_time_inited = 1;
    }
    QueryPerformanceCounter(&now);
    elapsed = now.QuadPart - s_perf_base.QuadPart;
    /* tics = elapsed / (freq / TICRATE) = elapsed * TICRATE / freq */
    return (int)(elapsed * TICRATE / s_perf_freq.QuadPart);
}

void I_Init(void)
{
    I_InitSound();
}

void I_Quit(void)
{
    D_QuitNetGame();
    I_ShutdownSound();
    I_ShutdownMusic();
    M_SaveDefaults();
    I_ShutdownGraphics();
    ExitProcess(0);
}

void I_WaitVBL(int count)
{
    Sleep(count * (1000 / 70));
}

void I_BeginRead(void)
{
}

void I_EndRead(void)
{
}

byte *I_AllocLow(int length)
{
    byte *mem = (byte *)malloc((size_t)length);
    if (mem)
        memset(mem, 0, (size_t)length);
    return mem;
}

extern boolean demorecording;

void I_Error(char *error, ...)
{
    va_list argptr;
    char buf[1024];
    char ext[4096];
    DWORD gle;
    int err;
    char *msg = NULL;
    int i, pos;

    va_start(argptr, error);
    vsnprintf(buf, sizeof(buf), error, argptr);
    va_end(argptr);

    fprintf(stderr, "Error: %s\n", buf);
    fflush(stderr);

    /* Add useful error codes and context on Windows. */
    gle = GetLastError();
    err = errno;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        gle,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&msg,
        0,
        NULL);

    pos = 0;
    pos += snprintf(ext + pos, sizeof(ext) - pos,
                    "%s\r\n\r\n"
                    "errno=%d\r\n"
                    "GetLastError=%lu (0x%08lX)%s%s\r\n\r\n"
                    "WADs:\r\n",
                    buf,
                    err,
                    gle, gle,
                    msg ? "\r\n" : "",
                    msg ? msg : "");

    for (i = 0; i < MAXWADFILES && wadfiles[i]; i++)
        pos += snprintf(ext + pos, sizeof(ext) - pos, "  %d) %s\r\n", i, wadfiles[i]);
    if (i == 0)
        pos += snprintf(ext + pos, sizeof(ext) - pos, "  (none)\r\n");

    if (msg)
        LocalFree(msg);

    /* Show error dialog so you can see what failed */
    MessageBoxA(NULL, ext, "DOOM Error", MB_OK | MB_ICONERROR);

    if (demorecording)
        G_CheckDemoStatus();

    D_QuitNetGame();
    I_ShutdownGraphics();
    ExitProcess((UINT)-1);
}

#endif /* _WIN32 */
