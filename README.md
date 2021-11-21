Sept. 29, 2021
-fix snr readings
-correct for timing delay from decoding (using modulo division on the clock)
-work on send ft8 and rx ft8
-add abort option during sending
-incremental timing (with kTime = 1 first, then kTime = 2)

Oct. 1, 2021
-add split frequency operation?

Oct. 2, 2021
-1 time_osr is giving better results than two?? FIXED
-make decoding time (and num blocks) a separate variable, not a function of number of samples FIXED

Oct. 12, 2021
-Write setup function under util, with frequency and RTC
-make decode_ft8 work with adc samples, not wav file
-Do multithreading
-find how to use quadrature filter + LPF

Oct. 18, 2021
-Logic timing-first if, then while, otherwise empty looping
-updating message list in GUI
-overclocking
-sending waterfall
-is dma a separate process?

