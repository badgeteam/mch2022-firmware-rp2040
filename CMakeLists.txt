cmake_minimum_required(VERSION 3.12)

# Project name
set(PROJECT mch2022_firmware)

# Board type
set(PICO_BOARD none) # MCH2022 badge
#set(PICO_BOARD pico) # Pico board (for debugging)

# Output filename

set(OUTPUT_FILENAME ${PROJECT})

# SDK
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(${PROJECT} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.3.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

pico_sdk_init()

# Bootloader

set(FLASHLOADER flashloader)

add_executable(${FLASHLOADER})

target_sources(${FLASHLOADER} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/flashloader/flashloader.c
        )

target_link_libraries(${FLASHLOADER} PRIVATE
        hardware_structs
        hardware_sync
        hardware_flash
        hardware_watchdog
        hardware_resets
        hardware_xosc
        hardware_clocks
        hardware_pll
        hardware_dma
        pico_platform
        pico_standard_link
        )

pico_add_uf2_output(${FLASHLOADER})
pico_set_program_name(${FLASHLOADER} ${FLASHLOADER})
target_compile_options(${FLASHLOADER} PRIVATE -Wall -Wextra -Wno-ignored-qualifiers -Os)

set(FLASHLOADER_UF2 ${CMAKE_CURRENT_BINARY_DIR}/${FLASHLOADER}.uf2)

# Application

set(FIRMWARE firmware)

#add_subdirectory(i2c_slave)

# Project source files
add_executable(${FIRMWARE}
    main.c
    usb/usb_descriptors.c
    usb/usb_cdc.c
    #usb_hid.c
    #i2c_peripheral.c
    #uart_task.c
    #webusb_task.c
    #stdio_cdc2.c
    #msc_disk.c
    #flashloader_interface.c
    #ws2812.c
    #lcd.c
)

target_include_directories(${FIRMWARE} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR},
    ${CMAKE_CURRENT_LIST_DIR}/usb
)

target_link_libraries(${FIRMWARE}
    pico_stdlib
    pico_unique_id
    hardware_watchdog
    hardware_flash
    hardware_uart
    hardware_pio
    hardware_pwm
    hardware_adc
    pico_multicore
    tinyusb_device
    tinyusb_board
)

#pico_generate_pio_header(${FIRMWARE} ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

pico_add_uf2_output(${FIRMWARE})
pico_add_hex_output(${FIRMWARE})

target_link_options(${FIRMWARE} PRIVATE -Wl,-z,max-page-size=4096,-Os)
pico_set_linker_script(${FIRMWARE} ${CMAKE_CURRENT_SOURCE_DIR}/memmap_default.ld)

set(FIRMWARE_UF2 ${CMAKE_CURRENT_BINARY_DIR}/${FIRMWARE}.uf2)

# Combined bootloader + firmware UF2

set(COMPLETE_UF2 ${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_FILENAME}.uf2)

find_package (Python3 REQUIRED COMPONENTS Interpreter)
add_custom_command(OUTPUT ${COMPLETE_UF2} DEPENDS ${FLASHLOADER} ${FIRMWARE}
        COMMENT "Building full UF2 image"
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/uf2tool.py
                -o ${COMPLETE_UF2} ${FLASHLOADER_UF2} ${FIRMWARE_UF2}
        )

add_custom_target(${PROJECT} ALL DEPENDS ${COMPLETE_UF2})

# Set up files for the release package

install(FILES
    ${COMPLETE_UF2}
    DESTINATION release
)

set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_GENERATOR "ZIP" "TGZ")
include(CPack)
