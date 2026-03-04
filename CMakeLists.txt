cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

project(universal_flash_nuke C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

add_executable(flash_nuke
        nuke.c
        flash.c
        )

target_link_libraries(flash_nuke
        pico_stdlib
        hardware_flash
        )

# Flash offset to start erasing from (keep bootloader area). Default: 0x2000 (8 KiB).
if(NOT DEFINED NUKE_BOOTLOADER_SIZE)
    set(NUKE_BOOTLOADER_SIZE 0x2000)
endif()

# Flash offset at which erasing stops. 0 = erase to end of flash.
if(NOT DEFINED NUKE_MAX_BYTES)
    set(NUKE_MAX_BYTES 0)
endif()

# Dummy PICO_FLASH_SIZE_BYTES so that flash_range_erase doesn't just assert
target_compile_definitions(flash_nuke PRIVATE
        PICO_FLASH_SIZE_BYTES=33554432
        NUKE_BOOTLOADER_SIZE=${NUKE_BOOTLOADER_SIZE}
        NUKE_MAX_BYTES=${NUKE_MAX_BYTES}
        )

# It doesn't make sense to run this program from flash. Always build a
# RAM-only binary.
pico_set_binary_type(flash_nuke no_flash)

pico_add_extra_outputs(flash_nuke)
