add_library(st7789 st7789.c st7789.h gfx.c gfx.h glcdfont.c)
target_link_libraries(st7789 pico_stdlib hardware_spi hardware_timer)