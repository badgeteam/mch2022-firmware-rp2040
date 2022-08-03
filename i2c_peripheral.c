/*
 * Copyright (c) 2022 Nicolai Electronics
 * Copyright (c) 2021 Valentin Milea <valentin.milea@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "i2c_peripheral.h"

#include <i2c_fifo.h>
#include <i2c_slave.h>
#include <pico/stdlib.h>
#include <stdio.h>
#include <string.h>

#include "RP2040.h"
#include "bsp/board.h"
#include "hardware.h"
#include "hardware/adc.h"
#include "hardware/structs/watchdog.h"
#include "hardware/watchdog.h"
#include "lcd.h"
#include "nec_transmit.h"
#include "pico/time.h"
#include "pico/unique_id.h"
#include "uart_task.h"
#include "version.h"
#include "ws2812.h"

static bool interrupt_target = false;
static bool interrupt_state  = false;
static bool interrupt_clear  = false;

static bool usb_mounted           = false;
static bool usb_suspended         = false;
static bool usb_rempote_wakeup_en = false;

static uint8_t webusb_mode      = 0;
static bool    webusb_interrupt = false;

static int ir_statemachine = -1;

static struct {
    uint8_t registers[256];
    bool    modified[256];
    uint8_t address;
    bool    write_in_progress;
} i2c_registers;

static const uint8_t i2c_controlled_gpios[] = {SAO_IO0_PIN, SAO_IO1_PIN, PROTO_0_PIN, PROTO_1_PIN};
static const uint8_t input1_gpios[]         = {BUTTON_HOME, BUTTON_MENU, BUTTON_START, BUTTON_ACCEPT, BUTTON_BACK, FPGA_CDONE};
static const uint8_t input2_gpios[]         = {BUTTON_JOY_A, BUTTON_JOY_B, BUTTON_JOY_C, BUTTON_JOY_D, BUTTON_JOY_E};

static absolute_time_t next_button_poll;
static absolute_time_t next_adc_read;
static uint8_t         next_adc_channel;

static const bool i2c_registers_read_only[256] = {
    true,  false, true,  false, false, false, true,  true,   // 0-7
    true,  true,  false, true,  true,  true,  true,  true,   // 8-15
    false, false, true,  false, false, true,  true,  true,   // 16-23
    true,  true,  true,  true,  true,  true,  true,  true,   // 24-31
    false, false, false, false, false, false, false, false,  // 32-39
    false, false, false, false, false, false, false, false,  // 40-47
    false, false, false, false, false, false, false, false,  // 48-55
    false, false, false, false, false, false, false, false,  // 56-63
    false, false, false, false, false, false, false, false,  // 64-71
    false, false, false, false, false, false, false, false,  // 72-79
    false, false, false, false, false, false, false, false,  // 80-87
    false, false, false, false, false, false, false, false,  // 88-95
    false, false, false, false, false, false, false, false,  // 96-103
    false, false, false, false, false, false, false, false,  // 104-111
    false, false, false, false, false, false, false, false,  // 112-119
    false, false, false, false, false, false, false, false,  // 120-127
    // ... (128-255)
};

void setup_i2c_peripheral(i2c_inst_t* i2c, uint8_t sda_pin, uint8_t scl_pin, uint8_t address, uint32_t baudrate, i2c_slave_handler_t handler) {
    gpio_init(sda_pin);
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_pull_up(sda_pin);

    gpio_init(scl_pin);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(scl_pin);

    i2c_init(i2c, baudrate);
    i2c_slave_init(i2c, address, handler);
}

void setup_i2c_registers(int param_ir_statemachine) {
    ir_statemachine = param_ir_statemachine;
    for (uint16_t reg = 0; reg < 256; reg++) {
        i2c_registers.registers[reg] = 0;
        i2c_registers.modified[reg]  = false;
    }

    i2c_registers.registers[I2C_REGISTER_FW_VER] = FW_VERSION;

    pico_unique_board_id_t id;
    pico_get_unique_board_id((pico_unique_board_id_t*) &i2c_registers.registers[I2C_REGISTER_UID0]);

    for (uint8_t index = 0; index < sizeof(i2c_controlled_gpios); index++) {
        gpio_init(i2c_controlled_gpios[index]);
        gpio_set_dir(i2c_controlled_gpios[index], false);
        gpio_set_input_enabled(input1_gpios[index], true);
    }

    for (uint8_t index = 0; index < sizeof(input1_gpios); index++) {
        gpio_init(input1_gpios[index]);
        gpio_set_dir(input1_gpios[index], false);
        gpio_set_input_enabled(input1_gpios[index], true);
        gpio_pull_up(input1_gpios[index]);
    }

    for (uint8_t index = 0; index < sizeof(input2_gpios); index++) {
        gpio_init(input2_gpios[index]);
        gpio_set_dir(input2_gpios[index], false);
        gpio_set_input_enabled(input2_gpios[index], true);
        gpio_pull_up(input2_gpios[index]);
    }

    gpio_init(FPGA_CDONE);
    gpio_set_dir(FPGA_CDONE, false);

    gpio_init(FPGA_RESET);
    gpio_set_dir(FPGA_RESET, true);
    gpio_put(FPGA_RESET, false);

    gpio_init(ESP32_INT_PIN);
    gpio_set_dir(ESP32_INT_PIN, false);

    gpio_init(BATT_CHRG_PIN);
    gpio_set_dir(BATT_CHRG_PIN, false);
    gpio_pull_up(BATT_CHRG_PIN);

    next_button_poll = get_absolute_time();

    next_adc_read    = get_absolute_time();
    next_adc_channel = 0;

    adc_set_temp_sensor_enabled(true);
}

void i2c_register_write(uint8_t reg, uint8_t value) {
    i2c_registers.registers[reg] = value;
    i2c_registers.modified[reg]  = true;
}

void __not_in_flash_func(i2c_slave_handler)(i2c_inst_t* i2c, i2c_slave_event_t event) {
    // In ISR context, don't block and quickly complete!
    switch (event) {
        case I2C_SLAVE_RECEIVE:
            if (!i2c_registers.write_in_progress) {
                i2c_registers.address           = i2c_read_byte(i2c);
                i2c_registers.write_in_progress = true;
            } else {
                if (!i2c_registers_read_only[i2c_registers.address]) {
                    i2c_registers.registers[i2c_registers.address] = i2c_read_byte(i2c);
                    i2c_registers.modified[i2c_registers.address]  = true;
                }
                i2c_registers.address++;
            }
            break;
        case I2C_SLAVE_REQUEST:
            i2c_registers.write_in_progress = false;
            i2c_write_byte(i2c, i2c_registers.registers[i2c_registers.address]);
            if (i2c_registers.address == I2C_REGISTER_INTERRUPT2) {
                interrupt_target                                 = false;
                interrupt_clear                                  = true;
                i2c_registers.registers[I2C_REGISTER_INTERRUPT1] = 0;
                i2c_registers.registers[I2C_REGISTER_INTERRUPT2] = 0;
            }
            i2c_registers.address++;
            break;
        case I2C_SLAVE_FINISH:
            i2c_registers.write_in_progress = false;
            break;
        default:
            break;
    }
}

#define BOOTLOADER_ENTRY_MAGIC 0xb105f00d

void i2c_handle_register_write(uint8_t reg, uint8_t value) {
    switch (reg) {
        case I2C_REGISTER_GPIO_DIR:
            for (uint8_t pin = 0; pin < sizeof(i2c_controlled_gpios); pin++) {
                gpio_set_dir(i2c_controlled_gpios[pin], (value & (1 << pin)) >> pin);
            }
            break;
        case I2C_REGISTER_GPIO_OUT:
            for (uint8_t pin = 0; pin < sizeof(i2c_controlled_gpios); pin++) {
                gpio_put(i2c_controlled_gpios[pin], (value & (1 << pin)) >> pin);
            }
            break;
        case I2C_REGISTER_FPGA:
            gpio_put(FPGA_RESET, (i2c_registers.registers[I2C_REGISTER_FPGA] & 0x01));
            fpga_loopback((i2c_registers.registers[I2C_REGISTER_FPGA] & 0x02));
            break;
        case I2C_REGISTER_LCD_BACKLIGHT:
            lcd_backlight(value);
            break;
        case I2C_REGISTER_ADC_TRIGGER:
            if (value & 0x01) {
                // To-do: read ADC
            }
            if (value & 0x02) {
                // To-do: read ADC
            }
            break;
        case I2C_REGISTER_BL_TRIGGER:
            if (value == 0xBE) {
                hw_clear_bits(&watchdog_hw->ctrl, WATCHDOG_CTRL_ENABLE_BITS);
                watchdog_hw->scratch[5] = BOOTLOADER_ENTRY_MAGIC;
                watchdog_hw->scratch[6] = ~BOOTLOADER_ENTRY_MAGIC;
                watchdog_reboot(0, 0, 0);
                while (1) {
                    tight_loop_contents();
                    asm("");
                }
            }
            break;
        case I2C_REGISTER_IR_TRIGGER:
            if (value == 0x01) {
                uint16_t address = i2c_registers.registers[I2C_REGISTER_IR_ADDRESS_LO] + (i2c_registers.registers[I2C_REGISTER_IR_ADDRESS_HI] << 8);
                uint16_t command = i2c_registers.registers[I2C_REGISTER_IR_COMMAND] + ((~i2c_registers.registers[I2C_REGISTER_IR_COMMAND]) << 8);
                nec_tx_extended(IR_PIO, ir_statemachine, address, command);
            }
            break;
        case I2C_REGISTER_WS2812_MODE:
            switch (value) {
                case 0x01:  // 24-bit (RGB) mode
                    ws2812_enable(false);
                    break;
                case 0x02:  // 32-bit (RGBW) mode
                    ws2812_enable(true);
                case 0x00:
                default:
                    ws2812_disable();
            }
            break;
        case I2C_REGISTER_WS2812_TRIGGER:
            {
                uint8_t length = i2c_registers.registers[I2C_REGISTER_WS2812_LENGTH];
                if (length > 10) length = 10;
                for (uint8_t i = 0; i < length; i++) {
                    uint32_t* value = (uint32_t*) &i2c_registers.registers[I2C_REGISTER_WS2812_LED0_DATA0 + (i * 4)];
                    ws2812_put(*value);
                }
                break;
            }
        default:
            break;
    };
}

void i2c_task() {
    bool busy = i2c_slave_transfer_in_progress(I2C_SYSTEM);
    if (!busy) {
        // Deal with IRQ first
        if (interrupt_clear) {
            gpio_set_dir(ESP32_INT_PIN, false);  // Input, pin has pull-up, idle state
            interrupt_state = false;
            interrupt_clear = false;
        } else if (interrupt_target != interrupt_state) {
            interrupt_state = interrupt_target;
            if (interrupt_target) {
                gpio_set_dir(ESP32_INT_PIN, true);  // Output, low, trigger interrupt on ESP32
                gpio_put(ESP32_INT_PIN, false);
            } else {
                gpio_set_dir(ESP32_INT_PIN, false);  // Input, pin has pull-up, idle state
            }
        }

        // Dispatch register writes
        for (uint16_t reg = 0; reg < 256; reg++) {
            if (i2c_registers.modified[reg]) {
                i2c_handle_register_write(reg, i2c_registers.registers[reg]);
                i2c_registers.modified[reg] = false;
            }
        }

        // Set USB state register
        i2c_registers.registers[I2C_REGISTER_USB] = (usb_mounted & 1) | ((usb_suspended & 1) << 1) | ((usb_rempote_wakeup_en & 1) << 2);

        // Set WebUSB mode register
        i2c_registers.registers[I2C_REGISTER_WEBUSB_MODE] = webusb_mode;
        if (webusb_interrupt) {
            webusb_interrupt = false;
            interrupt_target = true;
        }

        // Read GPIO pins
        uint8_t gpio_in_value = 0;
        for (uint8_t index = 0; index < sizeof(i2c_controlled_gpios); index++) {
            gpio_in_value |= gpio_get(i2c_controlled_gpios[index]) << index;
        }

        absolute_time_t now = get_absolute_time();

#ifdef NDEBUG
        if (now > next_button_poll) {
#else
        if (now._private_us_since_boot > next_button_poll._private_us_since_boot) {
#endif
            next_button_poll = delayed_by_ms(now, 30);

            // Read inputs
            uint8_t input1_value = 0;
            for (uint8_t index = 0; index < sizeof(input1_gpios); index++) {
                input1_value |= (!gpio_get(input1_gpios[index])) << index;
            }
            input1_value |= board_button_read() << 7;  // Select button
            if (input1_value != i2c_registers.registers[I2C_REGISTER_INPUT1]) interrupt_target = true;
            i2c_registers.registers[I2C_REGISTER_INTERRUPT1] |= (input1_value ^ i2c_registers.registers[I2C_REGISTER_INPUT1]);
            i2c_registers.registers[I2C_REGISTER_INPUT1] = input1_value;

            uint8_t input2_value = 0;
            for (uint8_t index = 0; index < sizeof(input2_gpios); index++) {
                input2_value |= (!gpio_get(input2_gpios[index])) << index;
            }
            if (input2_value != i2c_registers.registers[I2C_REGISTER_INPUT2]) interrupt_target = true;
            i2c_registers.registers[I2C_REGISTER_INTERRUPT2] |= (input2_value ^ i2c_registers.registers[I2C_REGISTER_INPUT2]);
            i2c_registers.registers[I2C_REGISTER_INPUT2] = input2_value;
        }

#ifdef NDEBUG
        if (now > next_adc_read) {  // Once every 250ms
#else
        if (now._private_us_since_boot > next_adc_read._private_us_since_boot) {  // Once every 250ms
#endif
            next_adc_read = delayed_by_ms(now, 250);

            i2c_registers.registers[I2C_REGISTER_CHARGING_STATE] = !gpio_get(BATT_CHRG_PIN);

            switch (next_adc_channel) {
                case 0:
                    {
                        uint16_t* reg_vusb = (uint16_t*) &i2c_registers.registers[I2C_REGISTER_ADC_VALUE_VUSB_LO];
                        adc_select_input(ANALOG_VUSB_ADC);
                        *reg_vusb        = adc_read();
                        next_adc_channel = 1;
                        break;
                    }
                case 1:
                    {
                        uint16_t* reg_vbat = (uint16_t*) &i2c_registers.registers[I2C_REGISTER_ADC_VALUE_VBAT_LO];
                        adc_select_input(ANALOG_VBAT_ADC);
                        *reg_vbat        = adc_read();
                        next_adc_channel = 2;
                        break;
                    }
                case 2:
                default:
                    {
                        uint16_t* reg_temp = (uint16_t*) &i2c_registers.registers[I2C_REGISTER_ADC_VALUE_TEMP_LO];
                        adc_select_input(ANALOG_TEMP_ADC);
                        *reg_temp        = adc_read();
                        next_adc_channel = 0;
                        break;
                    }
            }
        }
    }
}

void i2c_usb_set_mounted(bool mounted) { usb_mounted = mounted; }

void i2c_usb_set_suspended(bool suspended, bool remote_wakeup_en) {
    usb_suspended         = suspended;
    usb_rempote_wakeup_en = remote_wakeup_en;
}

void i2c_set_webusb_mode(uint8_t mode) {
    webusb_mode = mode;
    // webusb_interrupt = true;
}

void i2c_set_crash_debug_state(bool crashed, bool debug) { i2c_registers.registers[I2C_REGISTER_CRASH_DEBUG] = (crashed & 1) | ((debug << 1) & 2); }

void i2c_set_reset_attempted(bool attempted) { i2c_registers.registers[I2C_REGISTER_RESET_ATTEMPTED] = attempted; }

bool i2c_get_reset_allowed() { return !i2c_registers.registers[I2C_REGISTER_RESET_LOCK]; }
