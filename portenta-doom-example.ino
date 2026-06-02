/*
    Arduino wrapper for DoomGeneric
    Mouse and keyboard controls are not implemented at the moment.

    To use the internal QSPI flash as storage, run QSPIFormat
    sketch once to create the partitions, AccessFlashAsUSBDisk to expose the QSPI flash
    as a USB disk, copy DOOM1.WAD in the biggest partition, flash this sketch and you are ready to go :)

        Arduino wrapper for DoomGeneric
    Mouse and keyboard controls are not implemented at the moment.
    1. examples-->system_STM32H474_sytestem-->QSPIFormat
    2. examples-->USB Mass Storage-->AssesFlashAsUsbDisk
    3. move DOOM1.WAD to the second drive(5MB), I also put it on the 4th drive (7MB)

    4. examples-->system_STM32H474_sytestem-->QSPIfReadPartitions (Just to check partition sizes)
    5. install arduino portenta board v4.2.1 or lower
    6. Run examples-->Doom-->Doom

    7. Check your HDMI powered hub to see if it works. If you have the fancy cables you can use this website 
           https://hpssjellis.github.io/Arduino-Portenta-Doom/index.html
    8. Then load this version to try to get a joystick and grayscale OLED working
    9. Find your doom library, look in the compiler output on windows it should be about
           C:\Users\[USERNAME]]\AppData\Local\Arduino15\packages\arduino\hardware\mbed_portenta\4.2.1\libraries\doom\src
    10. Copy the src file as a backup, then go into the src file to look at the include files
    11. open with a text editor "src/doomgeneric_arduino.cpp" and make these changes...

*/

#include "QSPIFBlockDevice.h"
#include "FATFileSystem.h"
#include "MBRBlockDevice.h"
#include "doomgeneric.h"

QSPIFBlockDevice block_device;
// Comment previous line and uncomment these two if you want to store DOOM.WAD in an external SD card (FAT32 formatted)
// #include "SDMMCBlockDevice.h"
// SDMMCBlockDevice block_device;

mbed::MBRBlockDevice fs_data(&block_device, 2);
static mbed::FATFileSystem fs("fs");

extern "C" int main_wrapper(int argc, char **argv);
char*argv[] = {"/fs/doom", "-iwad", "/fs/DOOM1.WAD"};

void setup() {
  int err =  fs.mount(&fs_data);
  if (err) {
    printf("No filesystem found, please run AccessFlashAsUSBDisk sketch and copy DOOM1.WAD in the 2nd partition");
    pinMode(LEDB, OUTPUT);
    while (1) {
      digitalWrite(LEDB, LOW);
      delay(100);
      digitalWrite(LEDB, HIGH);
      delay(100);
    }
  }
  DIR *dir;
  struct dirent *ent;
  printf("try to open dir\n");
  if ((dir = opendir("/fs")) != NULL) {
    /* print all the files and directories within directory */
    while ((ent = readdir (dir)) != NULL) {
      printf ("%s\n", ent->d_name);
    }
    closedir (dir);
  } else {
    /* could not open directory */
    printf ("error\n");
  }
  main_wrapper(3, argv);
}

void loop() {
  // put your main code here, to run repeatedly:

}
