enable_language(C CXX ASM)
add_executable(RaspExample main.c)
add_library(lgraphics INTERFACE)
add_library(lsd_driver INTERFACE)
add_library(lsd_volume INTERFACE)
add_library(lsd_file INTERFACE)

target_sources(lgraphics PUBLIC graphics.c)
target_sources(lsd_driver PUBLIC sd_driver.c)
target_sources(lsd_volume PUBLIC sd_volume.c)
target_sources(lsd_file PUBLIC sd_file.c)

add_custom_command(
    TARGET RaspExample
    POST_BUILD
    COMMAND
    ${ARM_OBJDUMP} -hd RaspExample.elf > RaspExample.lst.map
)

target_link_libraries(RaspExample 
lsd_file
lsd_volume 
lsd_driver 
lgraphics 
pico_stdlib 
hardware_dma 
hardware_i2c 
hardware_spi)

