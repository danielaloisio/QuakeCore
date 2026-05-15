/*
Copyright (C) 2026 M3t4l

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see
<https://www.gnu.org/licenses/>.
*/

#include <windows.h>
#include <direct.h>
#include <io.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <SDL2/SDL.h>
#include "quakedef.h"

typedef enum {MS_WINDOWED, MS_FULLSCREEN, MS_FULLDIB, MS_UNINIT} modestate_t;
modestate_t modestate = MS_WINDOWED;

cvar_t _windowed_mouse = {"_windowed_mouse", "0", false};

void VID_LockBuffer(void)  {}
void VID_UnlockBuffer(void) {}

void IN_Accumulate(void) {}

DWORD gSndBufSize = 0;
void *pDSBuf      = NULL;

int  (PASCAL FAR *pWSAStartup)   (WORD, LPWSADATA)                = NULL;
int  (PASCAL FAR *pWSACleanup)   (void)                            = NULL;
int  (PASCAL FAR *pWSAGetLastError)(void)                           = NULL;
SOCKET (PASCAL FAR *psocket)     (int, int, int)                   = NULL;
int  (PASCAL FAR *pioctlsocket)  (SOCKET, long, u_long FAR *)      = NULL;
int  (PASCAL FAR *psetsockopt)   (SOCKET, int, int,
                                   const char FAR *, int)          = NULL;
int  (PASCAL FAR *precvfrom)     (SOCKET, char FAR *, int, int,
                                   struct sockaddr FAR *,
                                   int FAR *)                      = NULL;
int  (PASCAL FAR *psendto)       (SOCKET, const char FAR *, int,
                                   int, const struct sockaddr FAR *,
                                   int)                            = NULL;
int  (PASCAL FAR *pclosesocket)  (SOCKET)                          = NULL;
int  (PASCAL FAR *pgethostname)  (char FAR *, int)                 = NULL;
struct hostent FAR *(PASCAL FAR *pgethostbyname)(const char FAR *) = NULL;
struct hostent FAR *(PASCAL FAR *pgethostbyaddr)(const char FAR *,
                                                  int, int)        = NULL;
int  (PASCAL FAR *pgetsockname)  (SOCKET, struct sockaddr FAR *,
                                   int FAR *)                      = NULL;

qboolean    isDedicated;
int         nostdout = 0;
char       *basedir  = ".";
char       *cachedir = ".";

cvar_t  sys_linerefresh = {"sys_linerefresh","0"};

void Sys_DebugNumber(int y, int val) {}

void Sys_Printf(char *fmt, ...)
{
    va_list       argptr;
    char          text[1024];
    unsigned char *p;

    va_start(argptr, fmt);
    vsprintf(text, fmt, argptr);
    va_end(argptr);

    if (strlen(text) > sizeof(text))
        Sys_Error("memory overwrite in Sys_Printf");

    if (nostdout) return;

    for (p = (unsigned char *)text; *p; p++) {
        *p &= 0x7f;
        if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
            printf("[%02x]", *p);
        else
            putc(*p, stdout);
    }
}

void Sys_Quit(void)
{
    Host_Shutdown();
    fflush(stdout);
    exit(0);
}

void Sys_Init(void) {}

void Sys_Error(char *error, ...)
{
    va_list argptr;
    char    string[1024];

    va_start(argptr, error);
    vsprintf(string, error, argptr);
    va_end(argptr);
    fprintf(stderr, "Error: %s\n", string);

    Host_Shutdown();
    exit(1);
}

void Sys_Warn(char *warning, ...)
{
    va_list argptr;
    char    string[1024];

    va_start(argptr, warning);
    vsprintf(string, warning, argptr);
    va_end(argptr);
    fprintf(stderr, "Warning: %s", string);
}

int Sys_FileTime(char *path)
{
    struct stat buf;
    if (stat(path, &buf) == -1)
        return -1;
    return (int)buf.st_mtime;
}

void Sys_mkdir(char *path)
{
    _mkdir(path);
}

int Sys_FileOpenRead(char *path, int *handle)
{
    int         h;
    struct stat fileinfo;

    h = open(path, O_RDONLY | O_BINARY, 0666);
    *handle = h;
    if (h == -1)
        return -1;

    if (fstat(h, &fileinfo) == -1)
        Sys_Error("Error fstating %s", path);

    return (int)fileinfo.st_size;
}

int Sys_FileOpenWrite(char *path)
{
    int handle;

    handle = open(path, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0666);
    if (handle == -1)
        Sys_Error("Error opening %s: %s", path, strerror(errno));

    return handle;
}

int Sys_FileWrite(int handle, void *src, int count)
{
    return write(handle, src, count);
}

void Sys_FileClose(int handle)
{
    close(handle);
}

void Sys_FileSeek(int handle, int position)
{
    lseek(handle, position, SEEK_SET);
}

int Sys_FileRead(int handle, void *dest, int count)
{
    return read(handle, dest, count);
}

void Sys_DebugLog(char *file, char *fmt, ...)
{
    va_list     argptr;
    static char data[1024];
    int         fd;

    va_start(argptr, fmt);
    vsprintf(data, fmt, argptr);
    va_end(argptr);
    fd = open(file, O_WRONLY | O_CREAT | O_APPEND | O_BINARY, 0666);
    write(fd, data, (unsigned)strlen(data));
    close(fd);
}

void Sys_EditFile(char *filename) {}

double Sys_FloatTime(void)
{
    static Uint64 start    = 0;
    static Uint64 freq     = 0;
    Uint64        now;

    if (!freq)
        freq = SDL_GetPerformanceFrequency();

    now = SDL_GetPerformanceCounter();
    if (!start) {
        start = now;
        return 0.0;
    }
    return (double)(now - start) / (double)freq;
}

void Sys_LineRefresh(void) {}

char *Sys_ConsoleInput(void) { return NULL; }

void Sys_HighFPPrecision(void) {}
void Sys_LowFPPrecision(void)  {}

void Sys_MakeCodeWriteable(unsigned long startaddr, unsigned long length) {}

int main(int c, char **v)
{
    double       time, oldtime, newtime;
    quakeparms_t parms;
    extern int   vcrFile;
    extern int   recording;
    int          j;

    signal(SIGFPE, SIG_IGN);

    memset(&parms, 0, sizeof(parms));

    COM_InitArgv(c, v);
    parms.argc = com_argc;
    parms.argv = com_argv;

    parms.memsize = 16 * 1024 * 1024;

    j = COM_CheckParm("-mem");
    if (j)
        parms.memsize = (int)(Q_atof(com_argv[j + 1]) * 1024 * 1024);
    parms.membase = malloc(parms.memsize);
    parms.basedir = basedir;

    Host_Init(&parms);
    Sys_Init();

    if (COM_CheckParm("-nostdout"))
        nostdout = 1;

    oldtime = Sys_FloatTime() - 0.1;
    while (1) {
        newtime = Sys_FloatTime();
        time    = newtime - oldtime;

        if (cls.state == ca_dedicated) {
            if (time < sys_ticrate.value && (vcrFile == -1 || recording)) {
                SDL_Delay(1);
                continue;
            }
            time = sys_ticrate.value;
        }

        if (time > sys_ticrate.value * 2)
            oldtime = newtime;
        else
            oldtime += time;

        Host_Frame(time);

        if (sys_linerefresh.value)
            Sys_LineRefresh();
    }
}
