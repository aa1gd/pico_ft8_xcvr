add_library(pico_si5351 si5351.c si5351.h)
target_link_libraries(pico_si5351 pico_stdlib hardware_i2c hardware_timer hardware_clocks)
