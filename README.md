# Arduino-Portenta-Doom
Running DOOM on the Arduino PortentaH7


Repository started May 31, 2026


# Install Arduino Portenta Board version 4.2.1 unless you want to pull your remaining hair out.


Note: A stable power supply is essential a wall usb charger is needed! (Your computer USBA outlet, or a cell phone battery pack may not have enough power to stablize the hdmi output.)


## Overview

Correct board version 4.2.1 (up to 4.6.0 is broken) for Arduino PortentaH7, Run examples-->USB Mass Storage-->AssesFlashAsUsbDisk and grab DOOM1.WAD from https://doomwiki.org/wiki/DOOM1.WAD and put it into the 5MB folder which is folder #2,  load examples-->Doom-->Doom, find the Doom src in your arduino board install location and replace ```src/doomgeneric_arduino.cpp``` with the code above, connect joystick A0 x, A1 y and pins D2, D3, D4, D5 and connect all the HDMI hub cables


# web viewer

The webpage here is a webCam viewer-recorder which with the correct HDMI to usbA or C connector allows viewing DOOM or any HDMI or webcam in a recording studio only. It is a single page webpage if you want to vibe code it differently

https://hpssjellis.github.io/Arduino-Portenta-Doom/index.html


# Cables

You need a power providing HDMI hub. I had a few more cables when I got my UNOQ working on either the computer or my cell phone using the webCam viewer-recorder I made above.

<img width="821" height="683" alt="cables" src="https://github.com/user-attachments/assets/91481c9e-3aeb-4198-8d7d-4535e2fd8c18" />



## Generic steps to get DOOM running on auto mode with an HDMI connector

```
From the Arduino IDE running portenta board version 4.2.1

    1. examples-->system_STM32H474_sytestem-->QSPIFormat
    2. examples-->USB Mass Storage-->AssesFlashAsUsbDisk
     
    3. move DOOM1.WAD to the second drive(5MB), I also put it on the 4th drive (7MB). Get it from https://doomwiki.org/wiki/DOOM1.WAD

    4. examples-->system_STM32H474_sytestem-->QSPIfReadPartitions (Just to check partition sizes)
    5. install arduino portenta board v4.2.1 or lower
    6. Run examples-->Doom-->Doom

    7. Check your HDMI powered hub to see if it works. If you have the fancy cables you can use this website 
           https://hpssjellis.github.io/Arduino-Portenta-Doom/index.html
    8. Find your doom library, look in the compiler output on windows it should be about
           C:\Users\[USERNAME]]\AppData\Local\Arduino15\packages\arduino\hardware\mbed_portenta\4.2.1\libraries\doom\src
    9. Copy the src file as a backup, then go into the src file to look at the include files
    10. open with a text editor "src/doomgeneric_arduino.cpp" and that is where you work on DOOM. Controls should be doable. After making changes save that file but then go back and re-compile the ino file in the arduino IDE.


```

# Thst proves DOOM works on the protenta. 

Next stage getting the joystick and two buttons working in a sensible way.

The only file we edit is "doomgeneric_arduino.cpp" not the arduino IDE code.



