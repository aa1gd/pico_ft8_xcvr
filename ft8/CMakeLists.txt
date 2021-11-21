#add_library(ft8 constants.c constants.h crc.c crc.h decode.c decode.h encode.c encode.h ldpc.c ldpc.h pack.c pack.h text.c text.h unpack.c unpack.h)
add_library(ft8 constants.c constants.h crc.c crc.h decode.c decode.h encode.c encode.h ldpc.c ldpc.h pack.c pack.h text.c text.h unpack.c unpack.h decode_ft8.c decode_ft8.h gen_ft8.c gen_ft8.h)
target_link_libraries(ft8 pico_multicore)