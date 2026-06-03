// i_video_oled.cpp  --  doom-oled-v004
//
// C++ companion to i_video.c.
// Owns the Adafruit_SSD1327 object and exposes four plain-C functions
// that i_video.c calls.  Drop this file in the same src folder as i_video.c.
//
// The OLED framebuffer (128x128, 4-bit grayscale) is built row by row
// via myOledRow(), then pushed in one myOledFlush() call.
// A small status bar occupies rows 88-127 (below the 80-row game image).
//
// Wiring (software SPI):
//   DIN (MOSI)  D8
//   SCK         D9
//   CS          D7
//   DC          D6
//   RESET       -1  (not connected)

#include "Arduino.h"
#include <Adafruit_SSD1327.h>

#define MY_OLED_CLK   D9
#define MY_OLED_MOSI  D8
#define MY_OLED_CS    D7
#define MY_OLED_DC    D6
#define MY_OLED_RESET -1

#define MY_OLED_OUT_W   128
#define MY_OLED_OUT_H    80   // game image rows
#define MY_OLED_YOFFSET   0   // top of display

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
        myOled.setCursor(0, 0);
        myOled.println("DOOM v004 LOADING");
        myOled.display();
    }
}

// Called once per output row (0..79) with 128 nibble values (0-15).
// Draws the row directly using drawFastHLine with a solid colour per
// contiguous run -- reduces SPI calls vs one drawPixel per pixel.
void myOledRow(int row, uint8_t* pixels)
{
    if (!myOledReady) return;

    int x = 0;
    while (x < MY_OLED_OUT_W) {
        uint8_t shade = pixels[x];
        int run = 1;
        // Count how many consecutive pixels share the same shade
        while (x + run < MY_OLED_OUT_W && pixels[x + run] == shade) run++;
        // SSD1327 colour: library expects 0-15 for 4-bit modes
        myOled.fillRect(x, MY_OLED_YOFFSET + row, run, 1, shade);
        x += run;
    }
}

// Called after all 80 rows are submitted.
// Draws the status bar in the remaining 48 rows (y=80..127).
void myOledStatus(uint32_t frames, const char* keyLabel)
{
    if (!myOledReady) return;

    // Clear just the status area
    myOled.fillRect(0, 82, 128, 46, 0);

    // Divider line
    myOled.drawFastHLine(0, 81, 128, 8);

    myOled.setTextSize(1);
    myOled.setTextColor(SSD1327_WHITE);

    myOled.setCursor(0, 84);
    myOled.print("F:");
    myOled.println(frames);

    myOled.setCursor(0, 96);
    myOled.print(">");
    myOled.println(keyLabel);
}

void myOledFlush(void)
{
    if (!myOledReady) return;
    myOled.display();
}

} // extern "C"
