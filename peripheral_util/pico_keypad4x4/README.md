# 4x4 Matrix Keypad Raspberry Pi Pico C library 
Raspberry Pi Pico C library for controlling a 4x4 matrix keypad 

This library only works if you are using de C SDK for raspberry pi pico.

Add the following to your project CMakeLists File
```
# Add pico_keypad4x4 as a library to your project
add_library(keypad_4x4 STATIC <RELATIVE PATH to keypad_4x4.c>)

# Link required libraries to keypad_4x4
target_link_libraries(keypad_4x4
    pico_stdlib
    hardware_timer
)

# Link required libraries to main executable
target_link_libraries(main
    keypad_4x4
)
```

## Example code
[Video demo](https://youtu.be/Z83b9P72imE)
```
#include <stdio.h>
#include "hardware/timer.h"
#include "../lib/pico-keypad4x4/pico_keypad4x4.h"

uint columns[4] = { 18, 19, 20, 21 };
uint rows[4] = { 10, 11, 12, 13 };
char matrix[16] = {
    '1', '2' , '3', 'A',
    '4', '5' , '6', 'B',
    '7', '8' , '9', 'C',
    '*', '0' , '#', 'D'
};

int main() {
    stdio_init_all();
    pico_keypad_init(columns, rows, matrix);
    char key;
    while (true) {
        key = pico_keypad_get_key();
        printf("Key pressed: %c\n", key);
        busy_wait_us(500000);
    }
}
```

# Contact
Oswaldo Hernandez

E-mail: oswahdez00@gmail.com
