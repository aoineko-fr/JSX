JSX is a test tool for devices that connect through the MSX general purpose port (aka joystick port).

It allows you to on the fly change the value of the 3 output pins (pins 6, 7 and 8), and to see the value received on the input pins (1, 2, 3, 4, 6 and 7).
![jsx](https://github.com/user-attachments/assets/9407a171-523a-471e-bf4e-5c4a7804748f)

The format is a 48 KB ROM format because we need to skipper the BIOS interrupt handler (ISR) to avoid any interference.
