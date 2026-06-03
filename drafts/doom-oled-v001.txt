// doom-oled-v001
// Step 1: Disable HDMI/DSI output so the project compiles without
//         the Arduino_H7_Video / DSI display stack.
//         Nothing OLED-related is added yet -- that is the next step.
//
// Changes from original doomgeneric_arduino.cpp:
//   - Removed #include "Arduino_H7_Video.h"
//   - Removed #include "dsi.h"
//   - Removed Arduino_H7_Video display(640, 480, USBCVideo) object
//   - Removed uint32_t fbs[2]  (was unused; referenced DSI fb pool)
//   - DG_Init()            : removed display.begin() and the DSI memset/fb clear
//   - DG_OnPaletteReload() : stubbed (DMA2D_Init referenced DSI; not needed yet)
//   - DG_DrawFrame()       : stubbed -- no DMA2D, no DSI, just returns
//   All joystick / key-queue logic is completely unchanged.

#include "Arduino.h"
#include "mbed.h"

#define sleep _sleep

#include "doomkeys.h"
#include "m_argv.h"
#include "doomgeneric.h"

#include <stdio.h>
#include <unistd.h>

// ---------------------------------------------------------------------------
// Key queue (unchanged)
// ---------------------------------------------------------------------------

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex  = 0;

static unsigned char convertToDoomKey(unsigned char scancode)
{
    unsigned char key = 0;
    switch (scancode)
    {
    case 0x9C: case 0x1C: key = KEY_ENTER;      break;
    case 0x01:             key = KEY_ESCAPE;     break;
    case 0xCB: case 0x4B: key = KEY_LEFTARROW;  break;
    case 0xCD: case 0x4D: key = KEY_RIGHTARROW; break;
    case 0xC8: case 0x48: key = KEY_UPARROW;    break;
    case 0xD0: case 0x50: key = KEY_DOWNARROW;  break;
    case 0x1D:             key = KEY_FIRE;       break;
    case 0x39:             key = KEY_USE;        break;
    case 0x2A: case 0x36: key = KEY_RSHIFT;     break;
    case 0x15:             key = 'y';            break;
    default: break;
    }
    return key;
}

static void addKeyToQueue(int pressed, unsigned char keyCode)
{
    unsigned char key = convertToDoomKey(keyCode);
    unsigned short keyData = (pressed << 8) | key;
    s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
    s_KeyQueueWriteIndex++;
    s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

// ---------------------------------------------------------------------------
// Joystick / button input (unchanged)
// ---------------------------------------------------------------------------

#define MY_RUN_PIN   2      // D2: run/walk toggle button
#define MY_JOY_MS    50     // input poll interval ms
#define MY_COMBO_MS  80     // grace window ms for simultaneous press

#define MY_EVENT_QUEUE_SIZE 16

struct MyKeyEvent { int pressed; unsigned char doomKey; };
static MyKeyEvent  myEventQueue[MY_EVENT_QUEUE_SIZE];
static int         myEventHead = 0;
static int         myEventTail = 0;

static void myEnqueue(int pressed, unsigned char doomKey)
{
    int next = (myEventHead + 1) % MY_EVENT_QUEUE_SIZE;
    if (next == myEventTail) return;
    myEventQueue[myEventHead] = { pressed, doomKey };
    myEventHead = next;
}

static int myLastX    = 0;
static int myLastY    = 0;
static int myLastRawX = 0;
static int myLastRawY = 0;

static int      myA2Pressed   = 0;
static int      myA3Pressed   = 0;
static int      myComboActive = 0;
static uint32_t myA2PressTime = 0;
static uint32_t myA3PressTime = 0;
static int      myA2Fired     = 0;
static int      myA3Fired     = 0;
static int      myD2Pressed   = 0;

static uint32_t myLastJoyMs = 0;

static void myUpdateJoystick()
{
    uint32_t now = millis();
    if (now - myLastJoyMs < MY_JOY_MS) return;
    myLastJoyMs = now;

    int vx   = analogRead(A0);
    int vy   = analogRead(A1);
    int newX = (vx > 750) ? 1 : (vx < 250) ? -1 : 0;
    int newY = (vy > 750) ? 1 : (vy < 250) ? -1 : 0;
    int curX = (newX == myLastRawX) ? newX : 0;
    int curY = (newY == myLastRawY) ? newY : 0;
    myLastRawX = newX;
    myLastRawY = newY;

    if (curX != myLastX) {
        if (myLastX != 0) myEnqueue(0, myLastX > 0 ? KEY_RIGHTARROW : KEY_LEFTARROW);
        if (curX   != 0) myEnqueue(1, curX   > 0 ? KEY_RIGHTARROW : KEY_LEFTARROW);
    }
    if (curY != myLastY) {
        if (myLastY != 0) myEnqueue(0, myLastY < 0 ? KEY_UPARROW : KEY_DOWNARROW);
        if (curY   != 0) myEnqueue(1, curY   < 0 ? KEY_UPARROW : KEY_DOWNARROW);
    }
    myLastX = curX;
    myLastY = curY;

    int curA2 = (digitalRead(A2) == LOW) ? 1 : 0;
    int curA3 = (digitalRead(A3) == LOW) ? 1 : 0;

    if (curA2 && !myA2Pressed) myA2PressTime = now;
    if (curA3 && !myA3Pressed) myA3PressTime = now;
    myA2Pressed = curA2;
    myA3Pressed = curA3;

    int bothDown = curA2 && curA3;
    uint32_t gap = (myA2PressTime > myA3PressTime)
                   ? (myA2PressTime - myA3PressTime)
                   : (myA3PressTime - myA2PressTime);
    int withinGrace = (myA2PressTime > 0 && myA3PressTime > 0 && gap < MY_COMBO_MS);

    if ((bothDown || withinGrace) && !myComboActive) {
        if (myA2Fired) { myEnqueue(0, KEY_FIRE); myA2Fired = 0; }
        if (myA3Fired) { myEnqueue(0, KEY_USE);  myA3Fired = 0; }
        myEnqueue(1, KEY_ESCAPE);
        myComboActive = 1;
    }
    if (myComboActive && !curA2 && !curA3) {
        myEnqueue(0, KEY_ESCAPE);
        myComboActive = 0;
        myA2PressTime = 0;
        myA3PressTime = 0;
        myA2Fired     = 0;
        myA3Fired     = 0;
    }

    if (!myComboActive) {
        if (curA2 && !myA2Fired && !curA3 && (now - myA2PressTime >= MY_COMBO_MS)) {
            myEnqueue(1, KEY_FIRE);
            myA2Fired = 1;
        }
        if (!curA2 && myA2Fired) { myEnqueue(0, KEY_FIRE); myA2Fired = 0; }

        if (curA3 && !myA3Fired && !curA2 && (now - myA3PressTime >= MY_COMBO_MS)) {
            myEnqueue(1, KEY_USE);
            myA3Fired = 1;
        }
        if (!curA3 && myA3Fired) { myEnqueue(0, KEY_USE); myA3Fired = 0; }
    }

    int curD2 = (digitalRead(MY_RUN_PIN) == LOW) ? 1 : 0;
    if (curD2 && !myD2Pressed) {
        myEnqueue(1, KEY_RSHIFT);
        myEnqueue(0, KEY_RSHIFT);
    }
    myD2Pressed = curD2;
}

// ---------------------------------------------------------------------------
// DoomGeneric callbacks
// ---------------------------------------------------------------------------

void DG_Init()
{
    // Display initialisation removed -- will be added in next step.
    // Button / joystick pins still configured.
    pinMode(A2, INPUT_PULLUP);
    pinMode(A3, INPUT_PULLUP);
    pinMode(MY_RUN_PIN, INPUT_PULLUP);
}

void DG_OnPaletteReload()
{
    // DMA2D / CLUT setup removed along with DSI.
    // Will be replaced with OLED palette handling in a later step.
}

void DG_DrawFrame()
{
    // HDMI / DMA2D output removed.
    // DG_ScreenBuffer is still filled by Doom each frame;
    // we just discard it here until the OLED renderer is wired in.
}

void DG_SleepMs(uint32_t ms)
{
    delay(ms);
}

uint32_t DG_GetTicksMs()
{
    return millis();
}

int DG_GetKey(int* pressed, unsigned char* doomKey)
{
    myUpdateJoystick();

    if (myEventTail != myEventHead) {
        *pressed = myEventQueue[myEventTail].pressed;
        *doomKey  = myEventQueue[myEventTail].doomKey;
        myEventTail = (myEventTail + 1) % MY_EVENT_QUEUE_SIZE;
        return 1;
    }
    return 0;
}

void DG_SetWindowTitle(const char * title)
{
}
