/*
 * Copyright (c) 2022 Nicolai Electronics
 * Copyright (c) 2021 Valentin Milea <valentin.milea@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <i2c_fifo.h>
#include <i2c_slave.h>
#include "i2c_peripheral.h"

void setup_i2c_peripheral(i2c_inst_t *i2c, uint8_t sda_pin, uint8_t scl_pin, uint8_t address, uint32_t baudrate, i2c_slave_handler_t handler) {
    gpio_init(sda_pin);
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_pull_up(sda_pin);

    gpio_init(scl_pin);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(scl_pin);

    i2c_init(i2c, baudrate);
    i2c_slave_init(i2c, address, handler);
}

/* MCH2022 I2C peripheral */

struct {
    uint8_t registers[4];
    uint8_t address;
    bool transfer_in_progress;
} i2c_registers;

void setup_i2c_registers() {
    i2c_registers.registers[I2C_REGISTER_FW_VER] = 0xFF;
    i2c_registers.registers[I2C_REGISTER_BL_VER] = 0x01;
    i2c_registers.registers[I2C_REGISTER_BL_STATE] = 0x00;
    i2c_registers.registers[I2C_REGISTER_BL_CTRL] = 0x00;
}

void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event) {
    // In ISR context, don't block and quickly complete!
    switch (event) {
        case I2C_SLAVE_RECEIVE:
            if (!i2c_registers.transfer_in_progress) {
                i2c_registers.address = i2c_read_byte(i2c);
                i2c_registers.transfer_in_progress = true;
            } else {
                if (i2c_registers.address == I2C_REGISTER_BL_CTRL) {
                    i2c_registers.registers[i2c_registers.address] = i2c_read_byte(i2c);
                }
                i2c_registers.address++;
            }
            break;
        case I2C_SLAVE_REQUEST:
            i2c_write_byte(i2c, i2c_registers.registers[i2c_registers.address]);
            i2c_registers.address++;
            break;
        case I2C_SLAVE_FINISH:
            i2c_registers.transfer_in_progress = false;
            break;
        default:
            break;
    }
}

void i2c_bl_set_state(uint8_t state) {
    i2c_registers.registers[I2C_REGISTER_BL_STATE] = state;
}

bool i2c_bl_get_ctrl() {
    return i2c_registers.registers[I2C_REGISTER_BL_CTRL];
}
