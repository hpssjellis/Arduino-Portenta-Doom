// i_video_oled.cpp  --  doom-oled-v004b
//
// C++ companion to i_video.c.
// Drop in the same src folder as i_video.c.
//
// Display layout (128x128):
//   Rows   0-103  : Doom game image (128x104, scaled from 320x200)
//   Row  104      : divider line
//   Rows 105-127  : status bar (frame count + last keypress)

#include "Arduino.h"
#include <Adafruit_SSD1327.h>

#define MY_OLED_CLK   D9
#define MY_OLED_MOSI  D8
#define MY_OLED_CS    D7
#define MY_OLED_DC    D6
#define MY_OLED_RESET -1

#define MY_OLED_OUT_H  104   // must match i_video.c

static Adafruit_SSD1327 myOled(128, 128, MY_OLED_MOSI, MY_OLED_CLK,
                                MY_OLED_DC, MY_OLED_RESET, MY_OLED_CS);
static bool myOledReady = false;

extern "C" {

void myOledInit(void)
{
    if (myOled.begin(0x3D)) {
        myOledReady = true;
        myOled.clearDisplay();
        myOled.setTextSize(1);
        myOled.setTextColor(SSD1327_WHITE);
        myOled.setCursor(0, 108);
        myOled.println("DOOM v004 LOADING");
        myOled.display();
    }
}

// Called once per output row (0..103) with 128 nibble values (0-15).
// Groups consecutive same-shade pixels into fillRect calls to minimise
// SPI transactions (worst case 128 calls/row, typical much fewer).
void myOledRow(int row, uint8_t* pixels)
{
    if (!myOledReady) return;

    int x = 0;
    while (x < 128) {
        uint8_t shade = pixels[x];
        int run = 1;
        while (x + run < 128 && pixels[x + run] == shade) run++;
        myOled.fillRect(x, row, run, 1, (uint16_t)shade);
        x += run;
    }
}

// Draws status bar below the game image.
// Clears only rows 105-127 so the divider line at 104 persists.
void myOledStatus(uint32_t frames, const char* keyLabel)
{
    if (!myOledReady) return;

    // Divider (only needs drawing once but cheap to redraw)
    myOled.drawFastHLine(0, 104, 128, 6);

    // Clear status area
    myOled.fillRect(0, 106, 128, 22, 0);

    myOled.setTextSize(1);
    myOled.setTextColor(SSD1327_WHITE);

    myOled.setCursor(0, 107);
    myOled.print("F:");
    myOled.print(frames);

    myOled.setCursor(0, 117);
    myOled.print(">");
    myOled.print(keyLabel);
}

void myOledFlush(void)
{
    if (!myOledReady) return;
    myOled.display();
}

} // extern "C"
