// i_video.c  --  doom-oled-v004
//
// Changes from original:
//   - Added Adafruit_SSD1327 OLED initialisation (called from I_InitGraphics)
//   - Added grayscale frame renderer in I_FinishUpdate
//   - I_VideoBuffer_FB and the double-resolution blowup loop are REMOVED
//     (they were allocating 512 KB of heap and likely causing the panic)
//   - DG_ScreenBuffer is now pointed at I_VideoBuffer directly (8-bit indexed)
//     which is what DG_DrawFrame receives; DG_DrawFrame is still called but
//     does nothing (v001 stub) so this is safe
//   - All other functions unchanged
//
// OLED render strategy:
//   - Source: I_VideoBuffer, 320x200, 8-bit palette indices
//   - Output: 128x80 region on the 128x128 display (centred vertically,
//             24px status bar below)
//   - Scale: nearest-neighbour  ox*320/128, oy*200/80
//   - Color: palette RGB -> luma via (r*77 + g*150 + b*29)>>8, then >>4 for 4-bit
//   - Render: one drawFastHLine per output row (80 SPI calls per frame)
//             using fillRect per pixel-run of same shade would be ideal but
//             hline is the safe starting point
//   - Throttle: max 15fps to OLED (~67ms between frames) so game loop is
//               not dominated by SPI
//
// Wiring (software SPI, same as your test sketch):
//   DIN (MOSI)  D8
//   SCK         D9
//   CS          D7
//   DC          D6
//   RESET       not connected (-1)

static const char
rcsid[] = "$Id: i_x.c,v 1.6 1997/02/03 22:45:10 b1 Exp $";

#include "config.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_event.h"
#include "d_main.h"
#include "i_video.h"
#include "z_zone.h"
#include "tables.h"
#include "doomkeys.h"
#include "doomgeneric.h"

#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/types.h>

// ---------------------------------------------------------------------------
// SSD1327 OLED  (C++ object referenced via extern "C" wrappers below)
// ---------------------------------------------------------------------------
// i_video.c is compiled as C by this Doom port.
// The Adafruit_SSD1327 object lives in a small C++ shim at the bottom of
// this file inside an #ifdef __cplusplus block -- but since the whole file
// is C we instead declare it in a separate inline section using the
// Arduino "extern C++" pattern.
//
// Simplest approach that works with a C file: declare the three wrapper
// functions as extern, implement them in a tiny companion .cpp file called
// i_video_oled.cpp that sits in the same src folder.
// ---------------------------------------------------------------------------

// Forward declarations -- implemented in i_video_oled.cpp
#ifdef __cplusplus
extern "C" {
#endif

void myOledInit(void);
// Draw one row of the OLED frame.
// row   : 0-79  (output row on display, offset by MY_OLED_YOFFSET)
// pixels: array of 128 grayscale nibble values (0-15)
void myOledRow(int row, uint8_t* pixels);
void myOledFlush(void);
void myOledStatus(uint32_t frames, const char* keyLabel);

#ifdef __cplusplus
}
#endif

// ---------------------------------------------------------------------------
// FB / scaling state (kept for compatibility, no longer used for output)
// ---------------------------------------------------------------------------

struct FB_BitField
{
    uint32_t offset;
    uint32_t length;
};

struct FB_ScreenInfo
{
    uint32_t xres;
    uint32_t yres;
    uint32_t xres_virtual;
    uint32_t yres_virtual;
    uint32_t bits_per_pixel;
    struct FB_BitField red;
    struct FB_BitField green;
    struct FB_BitField blue;
    struct FB_BitField transp;
};

static struct FB_ScreenInfo s_Fb;
int fb_scaling = 1;
int usemouse   = 0;

struct color {
    uint32_t b:8;
    uint32_t g:8;
    uint32_t r:8;
    uint32_t a:8;
};

struct color colors[256];

void I_GetEvent(void);

// ---------------------------------------------------------------------------
// Video buffers
// ---------------------------------------------------------------------------

byte *I_VideoBuffer    = NULL;
// I_VideoBuffer_FB removed -- was causing heap panic (512 KB allocation)

boolean screensaver_mode = false;
boolean screenvisible;

float mouse_acceleration = 2.0;
int   mouse_threshold    = 10;
int   usegamma           = 2;

typedef struct { byte r; byte g; byte b; } col_t;

static uint16_t rgb565_palette[256];  // kept for I_GetPaletteIndex

// ---------------------------------------------------------------------------
// OLED render state (C-side)
// ---------------------------------------------------------------------------

#define MY_OLED_OUT_W    128
#define MY_OLED_OUT_H     80   // Doom content rows on the OLED
#define MY_OLED_INTERVAL  67   // ms between OLED frames (~15 fps)

static uint32_t myOledLastMs  = 0;
static uint32_t myFrameCount  = 0;
static char     myKeyLabel[32] = "---";
static uint32_t myKeyLabelMs  = 0;
#define MY_KEY_CLEAR_MS 2000

// Called from doomgeneric cpp side to record last keypress label.
// Declared here so doomgeneric_arduino.cpp can call it.
void myOledSetKey(const char* label, uint32_t nowMs)
{
    int i;
    for (i = 0; i < 31 && label[i]; i++) myKeyLabel[i] = label[i];
    myKeyLabel[i] = '\0';
    myKeyLabelMs = nowMs;
}

// ---------------------------------------------------------------------------
// Palette helpers
// ---------------------------------------------------------------------------

#define GFX_RGB565(r, g, b) ((((r & 0xF8) >> 3) << 11) | (((g & 0xFC) >> 2) << 5) | ((b & 0xF8) >> 3))
#define GFX_RGB565_R(color) ((0xF800 & color) >> 11)
#define GFX_RGB565_G(color) ((0x07E0 & color) >> 5)
#define GFX_RGB565_B(color) (0x001F & color)

// Precomputed 8-bit luma for each palette entry (0-255).
// Recomputed on every I_SetPalette call.
static uint8_t myLuma[256];

static void myBuildLuma(void)
{
    int i;
    for (i = 0; i < 256; i++) {
        // integer BT.601 luma, result 0-255
        uint32_t l = ((uint32_t)colors[i].r * 77u +
                      (uint32_t)colors[i].g * 150u +
                      (uint32_t)colors[i].b * 29u) >> 8;
        myLuma[i] = (uint8_t)(l & 0xFF);
    }
}

// ---------------------------------------------------------------------------
// I_InitGraphics
// ---------------------------------------------------------------------------

void I_InitGraphics(void)
{
    int i;

    memset(&s_Fb, 0, sizeof(struct FB_ScreenInfo));
    s_Fb.xres            = DOOMGENERIC_RESX;
    s_Fb.yres            = DOOMGENERIC_RESY;
    s_Fb.xres_virtual    = s_Fb.xres;
    s_Fb.yres_virtual    = s_Fb.yres;
    s_Fb.bits_per_pixel  = 16;
    s_Fb.blue.length     = 5;
    s_Fb.green.length    = 6;
    s_Fb.red.length      = 5;
    s_Fb.blue.offset     = 0;
    s_Fb.green.offset    = 5;
    s_Fb.red.offset      = 11;

    printf("I_InitGraphics: %dx%d doom screen %dx%d\n",
           s_Fb.xres, s_Fb.yres, SCREENWIDTH, SCREENHEIGHT);

    fb_scaling = s_Fb.xres / SCREENWIDTH;
    if (s_Fb.yres / SCREENHEIGHT < fb_scaling)
        fb_scaling = s_Fb.yres / SCREENHEIGHT;

    // Allocate only the internal Doom draw buffer (320*200 = 64 KB).
    // The old I_VideoBuffer_FB (512 KB) is gone.
    I_VideoBuffer = (byte*)Z_Malloc(SCREENWIDTH * SCREENHEIGHT, PU_STATIC, NULL);

    // Point DG_ScreenBuffer at the same buffer.  DG_DrawFrame is a stub
    // so nothing reads it, but the pointer must not be NULL.
    DG_ScreenBuffer = (uint32_t*)I_VideoBuffer;

    screenvisible = true;

    // Initialise OLED (no-op if wiring wrong; Doom still runs)
    myOledInit();

    extern int I_InitInput(void);
    I_InitInput();
}

void I_ShutdownGraphics(void)
{
    Z_Free(I_VideoBuffer);
}

void I_StartFrame(void)  {}
void I_StartTic(void)    { I_GetEvent(); }
void I_UpdateNoBlit(void){}

// ---------------------------------------------------------------------------
// I_FinishUpdate  --  main render path
// ---------------------------------------------------------------------------

void I_FinishUpdate(void)
{
    uint32_t now;
    int oy, ox;
    uint8_t row[MY_OLED_OUT_W];

    myFrameCount++;

    // Throttle OLED updates
    // millis() is available because this file is compiled into the
    // Arduino mbed environment.
    now = (uint32_t)millis();
    if (now - myOledLastMs < MY_OLED_INTERVAL) {
        DG_DrawFrame();   // still call stub so timing is consistent
        return;
    }
    myOledLastMs = now;

    // --- Scale 320x200 -> 128x80, convert palette index -> 4-bit luma ---
    // Nearest-neighbour: source x = ox*320/128, source y = oy*200/80
    for (oy = 0; oy < MY_OLED_OUT_H; oy++) {
        int sy = (oy * SCREENHEIGHT) / MY_OLED_OUT_H;  // 0..199
        for (ox = 0; ox < MY_OLED_OUT_W; ox++) {
            int sx  = (ox * SCREENWIDTH) / MY_OLED_OUT_W;  // 0..319
            uint8_t idx   = I_VideoBuffer[sy * SCREENWIDTH + sx];
            uint8_t luma4 = myLuma[idx] >> 4;  // 0-15
            row[ox] = luma4;
        }
        myOledRow(oy, row);
    }

    // Status bar area: key label
    const char* kl = (now - myKeyLabelMs < MY_KEY_CLEAR_MS) ? myKeyLabel : "---";
    myOledStatus(myFrameCount, kl);

    myOledFlush();

    DG_DrawFrame();  // stub, returns immediately
}

// ---------------------------------------------------------------------------
// I_ReadScreen
// ---------------------------------------------------------------------------

void I_ReadScreen(byte* scr)
{
    memcpy(scr, I_VideoBuffer, SCREENWIDTH * SCREENHEIGHT);
}

// ---------------------------------------------------------------------------
// I_SetPalette
// ---------------------------------------------------------------------------

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
    DG_OnPaletteReload();  // stub
}

int I_GetPaletteIndex(int r, int g, int b)
{
    int best = 0, best_diff = INT_MAX, diff, i;
    col_t color;
    for (i = 0; i < 256; i++) {
        color.r = GFX_RGB565_R(rgb565_palette[i]);
        color.g = GFX_RGB565_G(rgb565_palette[i]);
        color.b = GFX_RGB565_B(rgb565_palette[i]);
        diff = (r - color.r)*(r - color.r)
             + (g - color.g)*(g - color.g)
             + (b - color.b)*(b - color.b);
        if (diff < best_diff) { best = i; best_diff = diff; }
        if (diff == 0) break;
    }
    return best;
}

void I_BeginRead(void)              {}
void I_EndRead(void)                {}
void I_SetWindowTitle(char *title)  { DG_SetWindowTitle(title); }
void I_GraphicsCheckCommandLine(void) {}
void I_SetGrabMouseCallback(grabmouse_callback_t func) {}
void I_EnableLoadingDisk(void)      {}
void I_BindVideoVariables(void)     {}
void I_DisplayFPSDots(boolean dots_on) {}
void I_CheckIsScreensaver(void)     {}
