/*
 * Copyright (c) 2021 Valentin Milea <valentin.milea@gmail.com>
 * Copyright (c) 2022 Sylvain Munaut <tnt@246tNt.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <hardware/irq.h>
#include <i2c_fifo.h>
#include <i2c_slave.h>

typedef struct i2c_slave_t {
    i2c_inst_t         *i2c;
    i2c_slave_handler_t handler;
    enum { ST_IDLE, ST_START, ST_WRITE, ST_READ } state;
} i2c_slave_t;

static i2c_slave_t i2c_slaves[2];

static inline void finish_transfer(i2c_slave_t *slave) {
    if (slave->state != ST_IDLE) {
        slave->handler(slave->i2c, I2C_SLAVE_FINISH);
        slave->state = ST_IDLE;
    }
}

static void __not_in_flash_func(i2c_slave_irq_handler)(i2c_slave_t *slave) {
    i2c_inst_t *i2c = slave->i2c;
    i2c_hw_t   *hw  = i2c_get_hw(i2c);

    uint32_t intr_stat = hw->intr_stat;

    // Only keep events we care about
    intr_stat &= (I2C_IC_INTR_STAT_R_TX_ABRT_BITS | I2C_IC_INTR_STAT_R_START_DET_BITS | I2C_IC_INTR_STAT_R_RX_FULL_BITS | I2C_IC_INTR_STAT_R_RD_REQ_BITS |
                  I2C_IC_INTR_STAT_R_STOP_DET_BITS);

    // Process while we have something pending
    while (intr_stat) {
        // Precedence of events depends on current state
        switch (slave->state) {
            case ST_IDLE:
                // Process starts first
                if (intr_stat & I2C_IC_INTR_STAT_R_START_DET_BITS) {
                    // HW/SW clears
                    intr_stat &= ~I2C_IC_INTR_STAT_R_START_DET_BITS;
                    hw->clr_start_det;

                    // What to do next ?
                    slave->state = ST_START;
                }

                // Clear any pending aborts
                else if (intr_stat & I2C_IC_INTR_STAT_R_TX_ABRT_BITS) {
                    // HW/SW clears
                    intr_stat &= ~I2C_IC_INTR_STAT_R_TX_ABRT_BITS;
                    hw->clr_tx_abrt;
                }

                // Other events should not happen !!!
                else {
                    goto error;
                }
                break;

            case ST_START:
                // Write is the most likely
                if (intr_stat & I2C_IC_INTR_STAT_R_RX_FULL_BITS) {
                    slave->state = ST_WRITE;
                }

                // And then read
                else if (intr_stat & I2C_IC_INTR_STAT_R_RD_REQ_BITS) {
                    slave->state = ST_READ;
                }

                // Other events should not happen !!!
                else {
                    goto error;
                }
                break;

            case ST_WRITE:
                // Handle any pending bytes first
                if (intr_stat & I2C_IC_INTR_STAT_R_RX_FULL_BITS) {
                    // SW clears
                    intr_stat &= ~I2C_IC_INTR_STAT_R_RX_FULL_BITS;

                    // Call slave handler
                    while (i2c_get_read_available(i2c)) slave->handler(i2c, I2C_SLAVE_RECEIVE);
                }

                // Next is stop, indicating we're done for this transaction
                else if (intr_stat & I2C_IC_INTR_STAT_R_STOP_DET_BITS) {
                    // HW/SW clears
                    intr_stat &= ~I2C_IC_INTR_STAT_R_STOP_DET_BITS;
                    hw->clr_stop_det;

                    // Done !
                    finish_transfer(slave);
                }

                // Could also be a restart, in which case, we'll switch to reads
                else if (intr_stat & I2C_IC_INTR_STAT_R_START_DET_BITS) {
                    // HW/SW clears
                    intr_stat &= ~I2C_IC_INTR_STAT_R_START_DET_BITS;
                    hw->clr_start_det;

                    // We expect reads
                    slave->state = ST_READ;
                }

                // Could also be a read directly
                // (not sure why, but sometime we don't see the restart ?!?)
                else if (intr_stat & I2C_IC_INTR_STAT_R_RD_REQ_BITS) {
                    // We expect reads
                    slave->state = ST_READ;
                }

                // Other events should not happen !!!
                else {
                    goto error;
                }
                break;

            case ST_READ:
                // Handle any read requests first
                if (intr_stat & I2C_IC_INTR_STAT_R_RD_REQ_BITS) {
                    // HW/SW clears
                    intr_stat &= ~I2C_IC_INTR_STAT_R_RD_REQ_BITS;
                    hw->clr_rd_req;

                    // Call slave handler
                    slave->handler(i2c, I2C_SLAVE_REQUEST);
                }

                // Finally, it maybe stop, indicating we're done for this transaction
                else if (intr_stat & I2C_IC_INTR_STAT_R_STOP_DET_BITS) {
                    // HW/SW clears
                    intr_stat &= ~I2C_IC_INTR_STAT_R_STOP_DET_BITS;
                    hw->clr_stop_det;

                    // Done !
                    finish_transfer(slave);
                }

                // Other events should not happen !!!
                else {
                    goto error;
                }
                break;
        }
    }

    return;

error:
    // Try to clear any conditions
    if (intr_stat & I2C_IC_INTR_STAT_R_TX_ABRT_BITS) {
        hw->clr_tx_abrt;
    }
    if (intr_stat & I2C_IC_INTR_STAT_R_START_DET_BITS) {
        hw->clr_start_det;
    }
    if (intr_stat & I2C_IC_INTR_STAT_R_RX_FULL_BITS) {
        while (i2c_get_read_available(i2c)) i2c_read_byte(i2c);
    }
    if (intr_stat & I2C_IC_INTR_STAT_R_RD_REQ_BITS) {
        hw->clr_rd_req;
        i2c_write_byte(i2c, 0xff);
    }
    if (intr_stat & I2C_IC_INTR_STAT_R_STOP_DET_BITS) {
        hw->clr_stop_det;
    }

    // Reset to IDLE
    slave->state = ST_IDLE;
}

static void __not_in_flash_func(i2c0_slave_irq_handler)() { i2c_slave_irq_handler(&i2c_slaves[0]); }

static void __not_in_flash_func(i2c1_slave_irq_handler)() { i2c_slave_irq_handler(&i2c_slaves[1]); }

void i2c_slave_init(i2c_inst_t *i2c, uint8_t address, i2c_slave_handler_t handler) {
    assert(i2c == i2c0 || i2c == i2c1);
    assert(handler != NULL);

    uint         i2c_index = i2c_hw_index(i2c);
    i2c_slave_t *slave     = &i2c_slaves[i2c_index];
    slave->i2c             = i2c;
    slave->handler         = handler;
    slave->state           = ST_IDLE;

    // Note: The I2C slave does clock stretching implicitly after a RD_REQ, while the Tx FIFO is empty.
    // There is also an option to enable clock stretching while the Rx FIFO is full, but we leave it
    // disabled since the Rx FIFO should never fill up (unless slave->handler() is way too slow).
    i2c_set_slave_mode(i2c, true, address);

    i2c_hw_t *hw = i2c_get_hw(i2c);
    // unmask necessary interrupts
    hw->intr_mask = I2C_IC_INTR_MASK_M_RX_FULL_BITS | I2C_IC_INTR_MASK_M_RD_REQ_BITS | I2C_IC_RAW_INTR_STAT_TX_ABRT_BITS | I2C_IC_INTR_MASK_M_STOP_DET_BITS |
                    I2C_IC_INTR_MASK_M_START_DET_BITS;

    // enable interrupt for current core
    irq_set_exclusive_handler(i2c_index == 0 ? I2C0_IRQ : I2C1_IRQ, i2c_index == 0 ? i2c0_slave_irq_handler : i2c1_slave_irq_handler);
    irq_set_enabled(i2c_index == 0 ? I2C0_IRQ : I2C1_IRQ, true);
}

void i2c_slave_deinit(i2c_inst_t *i2c) {
    assert(i2c == i2c0 || i2c == i2c1);

    uint         i2c_index = i2c_hw_index(i2c);
    i2c_slave_t *slave     = &i2c_slaves[i2c_index];
    assert(slave->i2c == i2c);  // should be called after i2c_slave_init()

    slave->i2c     = NULL;
    slave->handler = NULL;
    slave->state   = ST_IDLE;

    irq_set_enabled(i2c_index == 0 ? I2C0_IRQ : I2C1_IRQ, false);
    irq_remove_handler(i2c_index == 0 ? I2C0_IRQ : I2C1_IRQ, i2c_index == 0 ? i2c0_slave_irq_handler : i2c1_slave_irq_handler);

    i2c_hw_t *hw  = i2c_get_hw(i2c);
    hw->intr_mask = I2C_IC_INTR_MASK_RESET;

    i2c_set_slave_mode(i2c, false, 0);
}

bool i2c_slave_transfer_in_progress(i2c_inst_t *i2c) {
    assert(i2c == i2c0 || i2c == i2c1);
    uint         i2c_index = i2c_hw_index(i2c);
    i2c_slave_t *slave     = &i2c_slaves[i2c_index];
    return slave->state != ST_IDLE;
}
