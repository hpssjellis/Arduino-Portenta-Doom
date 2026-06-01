# Arduino-Portenta-Doom
Running DOOM on the Arduino PortentaH7


Repository started May 31, 2026


# Install Arduino Portenta Board version 4.2.1 unless you want to pull your remaining hair out.


Note: A stable power supply is essential a wall usb charger is needed! (Your computer USBA outlet, or a cell phone battery pack may not have enough power to stablize the hdmi output.)


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
    3. move DOOM1.WAD to the second drive(5MB), I also put it on the 4th drive (7MB)

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



