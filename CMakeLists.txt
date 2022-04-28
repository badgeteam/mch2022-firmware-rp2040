cmake_minimum_required(VERSION 3.12)

# Project name
set(NAME mch2022_firmware)
set(BOOTLOADER mch2022_bootloader)
set(COMBINED mch2022_combined)

# Board type
set(PICO_BOARD none)

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

# Firmware
add_executable(${NAME}
    main.c
    usb_descriptors.c
    uart_task.c
    lcd.c
    i2c_peripheral.c
)

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
)

pico_add_extra_outputs(${NAME})

pico_set_linker_script(${NAME}  ${CMAKE_CURRENT_SOURCE_DIR}/firmware.ld)
