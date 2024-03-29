cmake_minimum_required(VERSION 3.12)

message("Build type: \"${CMAKE_BUILD_TYPE}\"")

# Project name
set(NAME rp2040_firmware)
set(BOOTLOADER rp2040_bootloader)

# Board type
set(PICO_BOARD none)

# Fixes that allow some MCH2022 badges with a slowly starting oscillator to boot properly
add_compile_definitions(PICO_BOOT_STAGE2_CHOOSE_GENERIC_03H=1 PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64)

# SDK
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(${NAME} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.3.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

pico_sdk_init()

add_subdirectory(i2c_slave)

# Bootloader

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("Building in debug mode, NOT building bootloader")
else ()
    add_executable(${BOOTLOADER} bootloader/main.c bootloader/i2c_peripheral.c)

    function(target_cl_options option)
        target_compile_options(${BOOTLOADER} PRIVATE ${option})
        target_link_options(${BOOTLOADER} PRIVATE ${option})
    endfunction()

    target_cl_options("-Os")
    target_cl_options("-ffunction-sections")
    target_cl_options("-fdata-sections")
    target_link_options(${BOOTLOADER} PRIVATE "LINKER:--gc-sections")

    pico_add_extra_outputs(${BOOTLOADER})
    pico_set_binary_type(${BOOTLOADER} copy_to_ram)

    set_target_properties(${BOOTLOADER} PROPERTIES COMPILE_FLAGS "-Wall")

    pico_set_linker_script(${BOOTLOADER} ${CMAKE_CURRENT_SOURCE_DIR}/bootloader/bootloader.ld)

    target_link_libraries(${BOOTLOADER}
                        pico_stdlib
                        hardware_dma
                        hardware_flash
                        hardware_structs
                        hardware_resets
                        i2c_slave
                        cmsis_core)

    pico_enable_stdio_usb(${BOOTLOADER} 1)
endif ()

# NEC infrared library
add_subdirectory(nec_transmit)

# Firmware
add_executable(${NAME}
    main.c
    usb_descriptors.c
    uart_task.c
    webusb_task.c
    lcd.c
    i2c_peripheral.c
    ws2812.c
)

pico_generate_pio_header(${NAME} ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("Debug mode: using default panic handler")
else ()
    message("Release mode: using custom panic handler")
    target_compile_definitions(${NAME} PUBLIC PICO_PANIC_FUNCTION=custom_panic)
endif ()

target_include_directories(${NAME} PUBLIC
        ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(${NAME}
    pico_stdlib
    pico_unique_id
    hardware_watchdog
    hardware_flash
    hardware_uart
    hardware_pio
    hardware_pwm
    hardware_adc
    i2c_slave
    tinyusb_device
    tinyusb_board
    cmsis_core
    nec_transmit
)

pico_add_extra_outputs(${NAME})

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("Linking firmware for standalone use")
else ()
    message("Linking firmware for use with bootloader")
    pico_set_linker_script(${NAME}  ${CMAKE_CURRENT_SOURCE_DIR}/firmware.ld)
endif ()
