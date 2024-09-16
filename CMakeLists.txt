cmake_minimum_required(VERSION 3.27)

include(C:/Develop/RaspberryProjects/pico-sdk/pico_sdk_init.cmake)


project(Rasp1Proj)


pico_sdk_init()

add_subdirectory(src)

find_program(ARM_OBJDUMP arm-none-eabi-objdump REQUIRED)
