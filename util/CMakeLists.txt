#add_library(util wave.c wave.h tx_ft8.c tx_ft8.h)
add_library(util tx_ft8.c tx_ft8.h rx_ft8.c rx_ft8.h GUI.c GUI.h)
target_link_libraries(util hardware_adc hardware_dma hardware_spi hardware_rtc pico_stdlib)
#may need to link pico keypad