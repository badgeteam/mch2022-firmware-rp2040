# MCH2022 badge RP2040 firmware

This firmware implements a co-processor firmware which makes the RP2040 function as both a dual USB to serial converter and an I2C slave device
which allows the ESP32 to read button states and control things like the FPGA control signals and the SAO GPIOs.

## To-do list

 - [x] Implement USB -> Serial for ESP32
 - [x] Implement USB -> Serial for FPGA
 - [x] Implement boot mode selection and reset logic for programming the ESP32
 - [x] Implement I2C peripheral
 - [x] Implement bootloader
 - [x] Implement LCD backlight control
 - [ ] Bootloader cleanup
 - [ ] Firmware cleanup
 - [ ] Testing
