RPi Pico FT8 Transciever

Code to run a standalone FT8 transceiver based on the new Raspberry Pi Pico RP2040 microcontroller. Implemented in C, based on Karlis Goba YL3JG's FT8 Library. Uses the RPi Pico C/C++ SDK.

Currently, it is able to decode live audio signals that are input into the ADC, displaying them on the LCD. Uses a ST7789 240x320 LCD display, 4x4 membrane keyboard, Si5351 clock generator.

To do list:
-fix snr readings
-add abort option during sending
-add split frequency operation
-find how to use DSP quadrature filter + LPF in software
-sending waterfall

