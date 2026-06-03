// i_video.c  --  doom-oled-v004b
//
// Changes from original:
//   - REMOVED: config.h, fcntl.h, sys/types.h  (caused nested include panic
//              in ARM newlib toolchain; none are used by our code)
//   - REMOVED: I_VideoBuffer_FB and double-resolution blowup (was 512 KB heap)
//   - ADDED:   myOledInit/Row/Status/Flush calls for SSD1327 grayscale render
//   - DG_ScreenBuffer pointed at I_VideoBuffer (valid non-null ptr, stub safe)
//   - Game image: 320x200 -> 128x104 nearest-neighbour, 4-bit luma
//   - Status bar: rows 106-127 (frame count + last key)
//   - OLED throttled to ~15fps (67ms) so game loop is not dominated by SPI
//
// Companion file i_video_oled.cpp must be in the same src folder.
//
// Wiring: MOSI=D8, SCK=D9, CS=D7, DC=D6, RESET=-1

static const char
rcsid[] = "$Id: i_x.c,v 1.6 1997/02/03 22:45:10 b1 Exp $";

// Doom internal headers
#include "v_video.h"
#include "m_argv.h"
#include "d_event.h"
#include "d_main.h"
#include "i_video.h"
#include "z_zone.h"
#include "tables.h"
#include "doomkeys.h"
#include "doomgeneric.h"

// Safe standard headers only (no fcntl, no sys/types, no config.h)
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

// ---------------------------------------------------------------------------
// Forward declarations for C++ OLED wrappers in i_video_oled.cpp
// ---------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

void myOledInit(void);
void myOledRow(int row, uint8_t* pixels);   // row 0..MY_OLED_OUT_H-1, 128 nibbles
void myOledFlush(void);
void myOledStatus(uint32_t frames, const char* keyLabel);

#ifdef __cplusplus
}
#endif

// ---------------------------------------------------------------------------
// FB info struct (kept so fb_scaling logic compiles; no fd/mmap used)
// ---------------------------------------------------------------------------

struct FB_BitField { uint32_t offset; uint32_t length; };

struct FB_ScreenInfo {
    uint32_t xres, yres, xres_virtual, yres_virtual, bits_per_pixel;
    struct FB_BitField red, green, blue, transp;
};

static struct FB_ScreenInfo s_Fb;
int fb_scaling = 1;
int usemouse   = 0;

// ---------------------------------------------------------------------------
// Palette colour struct -- also used by i_video_oled.cpp via extern
// ---------------------------------------------------------------------------

struct color { uint32_t b:8; uint32_t g:8; uint32_t r:8; uint32_t a:8; };
struct color colors[256];

void I_GetEvent(void);

// ---------------------------------------------------------------------------
// Video buffers
// ---------------------------------------------------------------------------

byte *I_VideoBuffer    = NULL;
// I_VideoBuffer_FB REMOVED (was 640*400*2 = 512 KB, caused heap panic)

boolean screensaver_mode = false;
boolean screenvisible;

float mouse_acceleration = 2.0;
int   mouse_threshold    = 10;
int   usegamma           = 2;

typedef struct { byte r; byte g; byte b; } col_t;
static uint16_t rgb565_palette[256];

// ---------------------------------------------------------------------------
// OLED render constants and state
// ---------------------------------------------------------------------------

#define MY_OLED_OUT_W     128
#define MY_OLED_OUT_H     104   // game image rows (leaves 24 rows for status)
#define MY_OLED_INTERVAL   67   // ms between OLED frames (~15 fps)
#define MY_KEY_CLEAR_MS  2000

static uint32_t myOledLastMs = 0;
static uint32_t myFrameCount = 0;
static char     myKeyLabel[32] = "---";
static uint32_t myKeyLabelMs  = 0;

// Call this from doomgeneric_arduino.cpp DG_GetKey to log keypresses.
void myOledSetKey(const char* label, uint32_t nowMs)
{
    int i;
    for (i = 0; i < 31 && label[i]; i++) myKeyLabel[i] = label[i];
    myKeyLabel[i] = '\0';
    myKeyLabelMs  = nowMs;
}

// ---------------------------------------------------------------------------
// Luma lookup table -- rebuilt on every palette change
// ---------------------------------------------------------------------------

static uint8_t myLuma[256];

static void myBuildLuma(void)
{
    int i;
    for (i = 0; i < 256; i++) {
        uint32_t l = ((uint32_t)colors[i].r * 77u +
                      (uint32_t)colors[i].g * 150u +
                      (uint32_t)colors[i].b * 29u) >> 8;
        myLuma[i] = (uint8_t)(l & 0xFF);
    }
}

// ---------------------------------------------------------------------------
// Palette macros
// ---------------------------------------------------------------------------

#define GFX_RGB565(r,g,b)   ((((r&0xF8)>>3)<<11)|(((g&0xFC)>>2)<<5)|((b&0xF8)>>3))
#define GFX_RGB565_R(c)     ((0xF800&(c))>>11)
#define GFX_RGB565_G(c)     ((0x07E0&(c))>>5)
#define GFX_RGB565_B(c)     (0x001F&(c))

// ---------------------------------------------------------------------------
// I_InitGraphics
// ---------------------------------------------------------------------------

void I_InitGraphics(void)
{
    memset(&s_Fb, 0, sizeof(struct FB_ScreenInfo));
    s_Fb.xres           = DOOMGENERIC_RESX;
    s_Fb.yres           = DOOMGENERIC_RESY;
    s_Fb.xres_virtual   = s_Fb.xres;
    s_Fb.yres_virtual   = s_Fb.yres;
    s_Fb.bits_per_pixel = 16;
    s_Fb.blue.length    = 5;  s_Fb.blue.offset  = 0;
    s_Fb.green.length   = 6;  s_Fb.green.offset = 5;
    s_Fb.red.length     = 5;  s_Fb.red.offset   = 11;

    printf("I_InitGraphics: %dx%d fb, %dx%d doom\n",
           s_Fb.xres, s_Fb.yres, SCREENWIDTH, SCREENHEIGHT);

    fb_scaling = s_Fb.xres / SCREENWIDTH;
    if (s_Fb.yres / SCREENHEIGHT < fb_scaling)
        fb_scaling = s_Fb.yres / SCREENHEIGHT;

    // 320*200 = 64 KB only; old 512 KB buffer is gone
    I_VideoBuffer = (byte*)Z_Malloc(SCREENWIDTH * SCREENHEIGHT, PU_STATIC, NULL);

    // DG_ScreenBuffer must be non-null; DG_DrawFrame is a stub so safe
    DG_ScreenBuffer = (uint32_t*)I_VideoBuffer;

    screenvisible = true;

    myOledInit();   // in i_video_oled.cpp; no-op if OLED absent

    extern int I_InitInput(void);
    I_InitInput();
}

void I_ShutdownGraphics(void) { Z_Free(I_VideoBuffer); }
void I_StartFrame(void)       {}
void I_StartTic(void)         { I_GetEvent(); }
void I_UpdateNoBlit(void)     {}

// ---------------------------------------------------------------------------
// I_FinishUpdate -- called by Doom every rendered frame
// ---------------------------------------------------------------------------

void I_FinishUpdate(void)
{
    int oy, ox;
    uint8_t row[MY_OLED_OUT_W];

    myFrameCount++;

    uint32_t now = (uint32_t)millis();
    if (now - myOledLastMs < MY_OLED_INTERVAL) {
        DG_DrawFrame();
        return;
    }
    myOledLastMs = now;

    // Scale 320x200 -> 128x104, nearest-neighbour, palette index -> 4-bit luma
    for (oy = 0; oy < MY_OLED_OUT_H; oy++) {
        int sy = (oy * SCREENHEIGHT) / MY_OLED_OUT_H;
        for (ox = 0; ox < MY_OLED_OUT_W; ox++) {
            int     sx    = (ox * SCREENWIDTH) / MY_OLED_OUT_W;
            uint8_t idx   = I_VideoBuffer[sy * SCREENWIDTH + sx];
            row[ox]       = myLuma[idx] >> 4;   // 0-15
        }
        myOledRow(oy, row);
    }

    const char* kl = (now - myKeyLabelMs < MY_KEY_CLEAR_MS) ? myKeyLabel : "---";
    myOledStatus(myFrameCount, kl);
    myOledFlush();

    DG_DrawFrame();   // v001 stub
}

// ---------------------------------------------------------------------------
// Remaining standard functions
// ---------------------------------------------------------------------------

void I_ReadScreen(byte* scr)
{
    memcpy(scr, I_VideoBuffer, SCREENWIDTH * SCREENHEIGHT);
}

void I_SetPalette(byte* palette)
{
    int i;
    for (i = 0; i < 256; i++) {
        colors[i].a = 0;
        colors[i].r = gammatable[usegamma][*palette++];
        colors[i].g = gammatable[usegamma][*palette++];
        colors[i].b = gammatable[usegamma][*palette++];
    }
    myBuildLuma();
    DG_OnPaletteReload();
}

int I_GetPaletteIndex(int r, int g, int b)
{
    int best = 0, best_diff = INT_MAX, diff, i;
    col_t color;
    for (i = 0; i < 256; i++) {
        color.r = GFX_RGB565_R(rgb565_palette[i]);
        color.g = GFX_RGB565_G(rgb565_palette[i]);
        color.b = GFX_RGB565_B(rgb565_palette[i]);
        diff = (r-color.r)*(r-color.r)
             + (g-color.g)*(g-color.g)
             + (b-color.b)*(b-color.b);
        if (diff < best_diff) { best = i; best_diff = diff; }
        if (diff == 0) break;
    }
    return best;
}

void I_BeginRead(void)                              {}
void I_EndRead(void)                                {}
void I_SetWindowTitle(char *title)                  { DG_SetWindowTitle(title); }
void I_GraphicsCheckCommandLine(void)               {}
void I_SetGrabMouseCallback(grabmouse_callback_t func) {}
void I_EnableLoadingDisk(void)                      {}
void I_BindVideoVariables(void)                     {}
void I_DisplayFPSDots(boolean dots_on)              {}
void I_CheckIsScreensaver(void)                     {}
