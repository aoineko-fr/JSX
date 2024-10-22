JSX is a test tool for devices that connect through the MSX general purpose port (aka joystick port).

It allows you to on the fly change the value of the 3 output pins (pins 6, 7 and 8), and to see the value received on the input pins (1, 2, 3, 4, 6 and 7).

Controls:
- `0`: toggle v-synch (enable by default),
- `1` and `2`: change the selected port (port A and port B),
- `6`, `7` and `8`: toggle low/high state on the corresponding pin for the selected port,
- `Ctrl` + `6`, `7` and `8`: toggle the auto-pulse on that pin.

The format is a 48 KB ROM because we need to skipper the BIOS interrupt handler (ISR) to avoid any interference.

![jsx](https://github.com/user-attachments/assets/9407a171-523a-471e-bf4e-5c4a7804748f)
