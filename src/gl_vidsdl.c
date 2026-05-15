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

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#ifndef _WIN32
#include <dlfcn.h>
#else
#ifndef GL_SHARED_TEXTURE_PALETTE_EXT
#define GL_SHARED_TEXTURE_PALETTE_EXT 0x81FB
#endif
#endif

#include <SDL2/SDL.h>
#include <GL/gl.h>

#include "quakedef.h"

#ifdef _WIN32
BINDTEXFUNCPTR bindTexFunc;
#endif

#define WARP_WIDTH  320
#define WARP_HEIGHT 200

static SDL_Window   *sdl_window  = NULL;
static SDL_GLContext sdl_glctx   = NULL;

static qboolean mouse_active = false;
static qboolean mouse_avail  = true;
static int mx, my;
static int old_mouse_x, old_mouse_y;

static cvar_t in_mouse  = {"in_mouse",  "1", false};
static cvar_t m_filter  = {"m_filter",  "0", false};
cvar_t        gl_ztrick = {"gl_ztrick", "1", false};
static cvar_t vid_mode  = {"vid_mode",  "0", false};

unsigned short d_8to16table[256];
unsigned       d_8to24table[256];
unsigned char  d_15to8table[65536];

static float vid_gamma = 0.7f;

int   texture_mode            = GL_LINEAR;
int   texture_extension_number = 1;
float gldepthmin, gldepthmax;

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

qboolean is8bit      = false;
qboolean isPermedia  = false;
qboolean gl_mtexable = false;

extern kbutton_t in_lookup, in_lookdown;
extern void KeyDown(kbutton_t *b);
extern void KeyUp(kbutton_t *b);

void (*qglColorTableEXT)(int, int, int, int, int, const void *) = NULL;
void (*qgl3DfxSetPaletteEXT)(GLuint *)                          = NULL;

void D_BeginDirectRect(int x, int y, byte *pbitmap, int width, int height) {}
void D_EndDirectRect(int x, int y, int width, int height) {}

static void signal_handler(int sig)
{
    printf("Received signal %d, exiting...\n", sig);
    Sys_Quit();
    exit(0);
}

static void InitSig(void)
{
#ifndef _WIN32
    signal(SIGHUP,  signal_handler);
#endif
    signal(SIGINT,  signal_handler);
#ifndef _WIN32
    signal(SIGQUIT, signal_handler);
#endif
    signal(SIGILL,  signal_handler);
#ifndef _WIN32
    signal(SIGTRAP, signal_handler);
    signal(SIGBUS,  signal_handler);
#endif
    signal(SIGTERM, signal_handler);
}

void VID_ShiftPalette(unsigned char *p) {}

void VID_SetPalette(unsigned char *palette)
{
    byte         *pal = palette;
    unsigned     *table = d_8to24table;
    unsigned      r, g, b, v;
    int           r1, g1, b1, dist, bestdist;
    unsigned      i, k;

    for (i = 0; i < 256; i++) {
        r = pal[0]; g = pal[1]; b = pal[2];
        pal += 3;
        v = (255u << 24) | (r << 0) | (g << 8) | (b << 16);
        *table++ = v;
    }
    d_8to24table[255] &= 0xffffff;

    for (i = 0; i < (1 << 15); i++) {
        r = ((i & 0x1F)   << 3) + 4;
        g = ((i & 0x03E0) >> 2) + 4;
        b = ((i & 0x7C00) >> 7) + 4;
        pal = (unsigned char *)d_8to24table;
        bestdist = 10000 * 10000;
        k = 0;
        for (v = 0; v < 256; v++, pal += 4) {
            r1 = (int)r - (int)pal[0];
            g1 = (int)g - (int)pal[1];
            b1 = (int)b - (int)pal[2];
            dist = r1*r1 + g1*g1 + b1*b1;
            if (dist < bestdist) { k = v; bestdist = dist; }
        }
        d_15to8table[i] = k;
    }
}

static int SDLKeyToQuake(SDL_Keycode sym)
{
    switch (sym) {
    case SDLK_PAGEUP:    return K_PGUP;
    case SDLK_PAGEDOWN:  return K_PGDN;
    case SDLK_HOME:      return K_HOME;
    case SDLK_END:       return K_END;
    case SDLK_LEFT:      return K_LEFTARROW;
    case SDLK_RIGHT:     return K_RIGHTARROW;
    case SDLK_DOWN:      return K_DOWNARROW;
    case SDLK_UP:        return K_UPARROW;
    case SDLK_ESCAPE:    return K_ESCAPE;
    case SDLK_RETURN:    return K_ENTER;
    case SDLK_KP_ENTER:  return K_ENTER;
    case SDLK_TAB:       return K_TAB;
    case SDLK_F1:        return K_F1;
    case SDLK_F2:        return K_F2;
    case SDLK_F3:        return K_F3;
    case SDLK_F4:        return K_F4;
    case SDLK_F5:        return K_F5;
    case SDLK_F6:        return K_F6;
    case SDLK_F7:        return K_F7;
    case SDLK_F8:        return K_F8;
    case SDLK_F9:        return K_F9;
    case SDLK_F10:       return K_F10;
    case SDLK_F11:       return K_F11;
    case SDLK_F12:       return K_F12;
    case SDLK_BACKSPACE: return K_BACKSPACE;
    case SDLK_DELETE:    return K_DEL;
    case SDLK_PAUSE:     return K_PAUSE;
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:    return K_SHIFT;
    case SDLK_LCTRL:
    case SDLK_RCTRL:     return K_CTRL;
    case SDLK_LALT:
    case SDLK_RALT:      return K_ALT;
    case SDLK_INSERT:    return K_INS;
    case SDLK_KP_MULTIPLY: return '*';
    case SDLK_KP_PLUS:   return '+';
    case SDLK_KP_MINUS:  return '-';
    case SDLK_KP_DIVIDE: return '/';
    default:
        if (sym >= 'A' && sym <= 'Z')
            return sym - 'A' + 'a';
        if (sym < 128)
            return sym;
        return 0;
    }
}

static void IN_ActivateMouse(void)
{
    if (!mouse_avail || !sdl_window || mouse_active)
        return;
    SDL_SetRelativeMouseMode(SDL_TRUE);
    mx = my = 0;
    mouse_active = true;
}

static void IN_DeactivateMouse(void)
{
    if (!mouse_avail || !mouse_active)
        return;
    SDL_SetRelativeMouseMode(SDL_FALSE);
    mouse_active = false;
}

static void HandleEvents(void)
{
    SDL_Event ev;

    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT:
            Sys_Quit();
            break;

        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            int key = SDLKeyToQuake(ev.key.keysym.sym);
            if (key)
                Key_Event(key, ev.type == SDL_KEYDOWN);
            break;
        }

        case SDL_MOUSEMOTION:
            if (mouse_active) {
                mx += ev.motion.xrel;
                my += ev.motion.yrel;
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            int btn = -1;
            if (ev.button.button == SDL_BUTTON_LEFT)   btn = 0;
            else if (ev.button.button == SDL_BUTTON_RIGHT)  btn = 1;
            else if (ev.button.button == SDL_BUTTON_MIDDLE) btn = 2;
            if (btn >= 0)
                Key_Event(K_MOUSE1 + btn, ev.type == SDL_MOUSEBUTTONDOWN);
            break;
        }

        case SDL_MOUSEWHEEL:
            if (ev.wheel.y > 0) {
                Key_Event(K_MWHEELUP, true);
                Key_Event(K_MWHEELUP, false);
            } else if (ev.wheel.y < 0) {
                Key_Event(K_MWHEELDOWN, true);
                Key_Event(K_MWHEELDOWN, false);
            }
            break;

        case SDL_WINDOWEVENT:
            if (ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                IN_DeactivateMouse();
            else if (ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
                IN_ActivateMouse();
            break;
        }
    }
}


void CheckMultiTextureExtensions(void)
{
    if (strstr(gl_extensions, "GL_SGIS_multitexture") && !COM_CheckParm("-nomtex")) {
#ifdef _WIN32
        qglMTexCoord2fSGIS  = (void *)SDL_GL_GetProcAddress("glMTexCoord2fSGIS");
        qglSelectTextureSGIS = (void *)SDL_GL_GetProcAddress("glSelectTextureSGIS");
#else
        void *prjobj = dlopen(NULL, RTLD_LAZY);
        if (!prjobj) return;

        qglMTexCoord2fSGIS  = dlsym(prjobj, "glMTexCoord2fSGIS");
        qglSelectTextureSGIS = dlsym(prjobj, "glSelectTextureSGIS");
        dlclose(prjobj);
#endif
        if (qglMTexCoord2fSGIS && qglSelectTextureSGIS) {
            Con_Printf("Multitexture extensions found.\n");
            gl_mtexable = true;
        }
    }
}

void GL_Init(void)
{
#ifdef _WIN32
    bindTexFunc = (BINDTEXFUNCPTR)glBindTexture;
#endif
    gl_vendor     = (const char *)glGetString(GL_VENDOR);
    gl_renderer   = (const char *)glGetString(GL_RENDERER);
    gl_version    = (const char *)glGetString(GL_VERSION);
    gl_extensions = (const char *)glGetString(GL_EXTENSIONS);

    Con_Printf("GL_VENDOR:     %s\n", gl_vendor);
    Con_Printf("GL_RENDERER:   %s\n", gl_renderer);
    Con_Printf("GL_VERSION:    %s\n", gl_version);

    CheckMultiTextureExtensions();

    glClearColor(1, 0, 0, 0);
    glCullFace(GL_FRONT);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.666f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glShadeModel(GL_FLAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

void GL_BeginRendering(int *x, int *y, int *width, int *height)
{
    *x = *y = 0;
    SDL_GetWindowSize(sdl_window, width, height);
}

void GL_EndRendering(void)
{
    glFlush();
    SDL_GL_SwapWindow(sdl_window);
}

qboolean VID_Is8bit(void) { return is8bit; }

void VID_Init8bitPalette(void)
{
#ifdef _WIN32
    void *prjobj = (void *)1;
#else
    void *prjobj = dlopen(NULL, RTLD_LAZY);
    if (!prjobj) return;
#endif

    if (strstr(gl_extensions, "3DFX_set_global_palette") &&
#ifdef _WIN32
        (qgl3DfxSetPaletteEXT = (void *)SDL_GL_GetProcAddress("gl3DfxSetPaletteEXT")) != NULL) {
#else
        (qgl3DfxSetPaletteEXT = dlsym(prjobj, "gl3DfxSetPaletteEXT")) != NULL) {
#endif

        GLubyte table[256][4];
        char   *oldpal = (char *)d_8to24table;
        glEnable(GL_SHARED_TEXTURE_PALETTE_EXT);
        for (int i = 0; i < 256; i++) {
            table[i][2] = *oldpal++;
            table[i][1] = *oldpal++;
            table[i][0] = *oldpal++;
            table[i][3] = 255;
            oldpal++;
        }
        qgl3DfxSetPaletteEXT((GLuint *)table);
        is8bit = true;
        Con_SafePrintf("8-bit GL extensions enabled.\n");

    } else if (strstr(gl_extensions, "GL_EXT_shared_texture_palette") &&
#ifdef _WIN32
               (qglColorTableEXT = (void *)SDL_GL_GetProcAddress("glColorTableEXT")) != NULL) {
#else
               (qglColorTableEXT = dlsym(prjobj, "glColorTableEXT")) != NULL) {
#endif

        char  thePalette[256 * 3];
        char *oldPalette = (char *)d_8to24table;
        char *newPalette = thePalette;
        glEnable(GL_SHARED_TEXTURE_PALETTE_EXT);
        for (int i = 0; i < 256; i++) {
            *newPalette++ = *oldPalette++;
            *newPalette++ = *oldPalette++;
            *newPalette++ = *oldPalette++;
            oldPalette++;
        }
        qglColorTableEXT(GL_SHARED_TEXTURE_PALETTE_EXT, GL_RGB, 256,
                         GL_RGB, GL_UNSIGNED_BYTE, thePalette);
        is8bit = true;
        Con_SafePrintf("8-bit GL extensions enabled.\n");
    }

#ifndef _WIN32
    dlclose(prjobj);
#endif
}

static void Check_Gamma(unsigned char *pal)
{
    int           i;
    unsigned char palette[768];
    float         f, inf;

    i = COM_CheckParm("-gamma");
    if (i == 0)
        vid_gamma = 0.7f;
    else
        vid_gamma = Q_atof(com_argv[i + 1]);

    for (i = 0; i < 768; i++) {
        f = powf((pal[i] + 1) / 256.0f, vid_gamma);
        inf = f * 255.0f + 0.5f;
        if (inf < 0)   inf = 0;
        if (inf > 255) inf = 255;
        palette[i] = (unsigned char)inf;
    }
    memcpy(pal, palette, sizeof(palette));
}


void VID_Init(unsigned char *palette)
{
    int      i;
    int      width  = 640, height = 480;
    qboolean fullscreen = true;
    char     gldir[MAX_OSPATH];

    Cvar_RegisterVariable(&vid_mode);
    Cvar_RegisterVariable(&in_mouse);
    Cvar_RegisterVariable(&m_filter);
    Cvar_RegisterVariable(&gl_ztrick);

    vid.maxwarpwidth  = WARP_WIDTH;
    vid.maxwarpheight = WARP_HEIGHT;
    vid.colormap  = host_colormap;
    vid.fullbright = 256 - LittleLong(*((int *)vid.colormap + 2048));

    if (COM_CheckParm("-window"))
        fullscreen = false;
    if ((i = COM_CheckParm("-width")) != 0)
        width  = atoi(com_argv[i + 1]);
    if ((i = COM_CheckParm("-height")) != 0)
        height = atoi(com_argv[i + 1]);

    if ((i = COM_CheckParm("-conwidth")) != 0)
        vid.conwidth = Q_atoi(com_argv[i + 1]);
    else
        vid.conwidth = 640;

    vid.conwidth &= 0xfff8;
    if (vid.conwidth < 320) vid.conwidth = 320;
    vid.conheight = vid.conwidth * 3 / 4;

    if ((i = COM_CheckParm("-conheight")) != 0)
        vid.conheight = Q_atoi(com_argv[i + 1]);
    if (vid.conheight < 200) vid.conheight = 200;

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        Sys_Error("SDL_Init failed: %s", SDL_GetError());

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,     8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,   8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,    8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,   24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
    if (fullscreen)
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    sdl_window = SDL_CreateWindow("QuakeCore",
                                  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  width, height, flags);
    if (!sdl_window)
        Sys_Error("SDL_CreateWindow failed: %s", SDL_GetError());

    sdl_glctx = SDL_GL_CreateContext(sdl_window);
    if (!sdl_glctx)
        Sys_Error("SDL_GL_CreateContext failed: %s", SDL_GetError());

    SDL_GL_MakeCurrent(sdl_window, sdl_glctx);
    SDL_GL_SetSwapInterval(1);

    SDL_GL_GetDrawableSize(sdl_window, &width, &height);

    if (vid.conheight > height) vid.conheight = height;
    if (vid.conwidth  > width)  vid.conwidth  = width;
    vid.width  = vid.conwidth;
    vid.height = vid.conheight;
    vid.aspect = ((float)vid.height / (float)vid.width) * (320.0f / 240.0f);
    vid.numpages = 2;

    InitSig();
    GL_Init();

    sprintf(gldir, "%s/glquake", com_gamedir);
    Sys_mkdir(gldir);

    Check_Gamma(palette);
    VID_SetPalette(palette);
    VID_Init8bitPalette();

    Con_SafePrintf("Video mode %dx%d initialized.\n", width, height);

    vid.recalc_refdef = 1;
}

void VID_Shutdown(void)
{
    IN_DeactivateMouse();
    if (sdl_glctx) { SDL_GL_DeleteContext(sdl_glctx); sdl_glctx = NULL; }
    if (sdl_window) { SDL_DestroyWindow(sdl_window); sdl_window = NULL; }
    SDL_Quit();
}

void Sys_SendKeyEvents(void)
{
    HandleEvents();
}

void Force_CenterView_f(void)
{
    cl.viewangles[PITCH] = 0;
}

void IN_Init(void)
{
    Cvar_RegisterVariable(&in_mouse);
    Cvar_RegisterVariable(&m_filter);
    mouse_avail = true;
}

void IN_Shutdown(void)
{
    IN_DeactivateMouse();
}

void IN_Commands(void)
{
    if (!sdl_window) return;

    if (key_dest == key_game)
        IN_ActivateMouse();
    else
        IN_DeactivateMouse();
}

void IN_MouseMove(usercmd_t *cmd)
{
    if (!mouse_avail) return;

    if (m_filter.value) {
        mx = (mx + old_mouse_x) / 2;
        my = (my + old_mouse_y) / 2;
    }
    old_mouse_x = mx;
    old_mouse_y = my;

    mx = (int)(mx * sensitivity.value);
    my = (int)(my * sensitivity.value);

    if ((in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1)))
        cmd->sidemove += m_side.value * mx;
    else
        cl.viewangles[YAW] -= m_yaw.value * mx;

    if (in_mlook.state & 1)
        V_StopPitchDrift();

    if ((in_mlook.state & 1) && !(in_strafe.state & 1)) {
        cl.viewangles[PITCH] += m_pitch.value * my;
        if (cl.viewangles[PITCH] >  80) cl.viewangles[PITCH] =  80;
        if (cl.viewangles[PITCH] < -70) cl.viewangles[PITCH] = -70;
    } else {
        if (my < 0) {
            KeyDown(&in_lookup);
            KeyUp(&in_lookup);
        } else if (my > 0) {
            KeyDown(&in_lookdown);
            KeyUp(&in_lookdown);
        }
       /* if ((in_strafe.state & 1) && noclip_anglehack)
            cmd->upmove -= m_forward.value * my;
        else
            cmd->forwardmove -= m_forward.value * my;*/
    }
    mx = my = 0;
}

void IN_Move(usercmd_t *cmd)
{
    IN_MouseMove(cmd);
}

void VID_HandlePause(qboolean pause)
{
    if (pause)
        IN_DeactivateMouse();
    else
        IN_ActivateMouse();
}
