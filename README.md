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

## License information

The included bootloader is based on [RP2040 serial bootloader](https://github.com/usedbytes/rp2040-serial-bootloader) by Brian Starkey, licensed under BSD-3-Clause license.

The I2C peripheral function is based on [pico_i2c_slave](https://github.com/vmilea/pico_i2c_slave) by Valentin Milea, MIT license.

The USB descriptor is based on the example from the tinyusb library by Ha Thach licensed under MIT license.

The makefile has been provided by Jana Marie Hemsing under MIT license.

The other parts of the firmware are original work by [Renze Nicolai (Nicolai Electronics)](https://nicolaielectronics.nl) licensed under MIT license.