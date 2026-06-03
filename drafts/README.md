Here is where I try stuff. Looks like we are only editing the .cpp file so that does make things easier. my lastest attempts are:


newkeys##-cpp.txt  where the higher number is the latest attempt and lets hope is the better version

newkeys06-cpp.txt kind of works

newkeys09-cpp.txt Works except not sure if run mode is fully on as the joystick button does nothing, but you move fairly fast.
joystick-test06.txt really helped spot which input was giving trouble and to use the digital pins instead of the analog pins.

Working on newkeys10-cpp.txt to fix the run command to toggle run/walk


Next lets mess with the grayscale OLED

doom-oled-v001.txt uses the joystick etc but now trying to do some grayscale OLED. This code just stops the hdmi, nothing else.

So far pins are:

A0 joystick X  
A1 joystick y  
D2 joystick button for run  
D3 button menu choose  
D4 button open/close doors  
D5 button Fire.  

<img width="2572" height="2572" alt="portenta" src="https://github.com/user-attachments/assets/5744032a-4190-4bfd-8be1-211f91708748" />



