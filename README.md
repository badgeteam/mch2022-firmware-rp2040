# MCH2022 badge RP2040 firmware

This firmware implements a co-processor firmware which makes the RP2040 function as both a dual USB to serial converter and an I2C slave device
which allows the ESP32 to read button states and control things like the FPGA control signals and the SAO GPIOs.

## To-do list




# mch2022-firmware-rp2040

### todo
 - [ ] Implement USB -> Serial
  - [x] Build USB cdc
  - [ ] Build the serial part
  - [ ] Implement RST and EN lines for ESP32
  - [ ] Implement as full functioning ESP32 programmer
 - [ ] Implement I2C device
 - [ ] LCD control
  - [ ] Add RST + Mode IO
  - [ ] Figure out and implement reset/switching logic
