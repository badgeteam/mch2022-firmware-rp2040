#include "bsp/board.h"
#include "tusb.h"
#include "i2c_peripheral.h"

#include "hardware/dma.h"
#include "hardware/structs/dma.h"

#include "hardware.h"
#include "uart_task.h"

typedef struct {
    uint32_t magic;
    uint32_t identifier;
    uint32_t command;
    uint32_t payload_length;
    uint32_t payload_crc;
} msc_packet_header_t;

typedef struct {
    uint32_t magic;
    uint32_t identifier;
    uint32_t response;
    uint32_t payload_length;
    uint32_t payload_crc;
} msc_response_header_t;

typedef struct {
    uint32_t lun;
    uint32_t lba;
    uint32_t offset;
    uint32_t length;
} msc_payload_t;

typedef struct {
    msc_packet_header_t header;
    msc_payload_t payload;
} msc_packet_t;

#define MSC_CMD_SYNC (('S' << 0) | ('Y' << 8) | ('N' << 16) | ('C' << 24))  // Echo back empty response
#define MSC_CMD_PING (('P' << 0) | ('I' << 8) | ('N' << 16) | ('G' << 24))  // Echo payload back to PC
#define MSC_CMD_READ (('R' << 0) | ('E' << 8) | ('A' << 16) | ('D' << 24))  // Read block
#define MSC_CMD_WRIT (('W' << 0) | ('R' << 8) | ('I' << 16) | ('T' << 24))  // Write block
#define MSC_ANS_OKOK (('O' << 0) | ('K' << 8) | ('O' << 16) | ('K' << 24))

static const uint32_t msc_packet_magic = 0xFEEDF00D;

// whether host does safe-eject
static bool ejected = false;

#define README_CONTENTS \
"This is tinyusb's MassStorage Class demo.\r\n\r\n\
If you find any bugs or get any questions, feel free to file an\r\n\
issue at github.com/hathach/tinyusb"

enum {
  DISK_BLOCK_NUM  = 16, // 8KB is the smallest size that windows allow to mount
  DISK_BLOCK_SIZE = 512
};

uint8_t msc_buffer[4096 + sizeof(msc_packet_t)];

// ptr must be 4-byte aligned and len must be a multiple of 4
static uint32_t calc_crc32(void *ptr, uint32_t len) {
    uint32_t dummy_dest, crc;

    int                channel = dma_claim_unused_channel(true);
    dma_channel_config c       = dma_channel_get_default_config(channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_sniff_enable(&c, true);

    // Seed the CRC calculation
    dma_hw->sniff_data = 0xffffffff;

    // Mode 1, then bit-reverse the result gives the same result as
    // golang's IEEE802.3 implementation
    dma_sniffer_enable(channel, 0x1, true);
    dma_hw->sniff_ctrl |= DMA_SNIFF_CTRL_OUT_REV_BITS;

    dma_channel_configure(channel, &c, &dummy_dest, ptr, len / 4, true);

    dma_channel_wait_for_finish_blocking(channel);

    // Read the result before resetting
    crc = dma_hw->sniff_data ^ 0xffffffff;

    dma_sniffer_disable();
    dma_channel_unclaim(channel);

    return crc;
}

// Invoked to determine max LUN
uint8_t tud_msc_get_maxlun_cb(void) {
  return 2; // dual LUN
}

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
  const char vid[] = "MCH2022";
  const char pid0[] = "Internal flash";
  const char pid1[] = "SD card";
  const char rev[] = "1337";

  memcpy(vendor_id  , vid, strlen(vid));
  memcpy(product_id , (lun == 1) ? pid1 : pid0, strlen((lun == 1) ? pid1 : pid0));
  memcpy(product_rev, rev, strlen(rev));
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
  uint8_t control = i2c_get_msc_control();
  bool ready = false;

  if (lun == 0) { // Internal flash
    ready = (control & 0x02) >> 1;
  } else if (lun == 1) { // SD card
    ready = (control & 0x04) >> 2;
  }

  if (!ready) {
    tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
  }
  return ready;
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
  *block_count = i2c_get_msc_block_count(lun);
  *block_size  = i2c_get_msc_block_size(lun);
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
  (void) lun;
  (void) power_condition;

  if (load_eject) {
    if (start) {
      // load disk storage
    } else {
      // unload disk storage
      ejected = true;
    }
  }
  return true;
}

void flush_uart() {
    int waiting = uart_is_readable(UART_ESP32);
    while (waiting > 0) {
        uart_getc(UART_ESP32);
        waiting--;
    }
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    char logbuf[64];
    sprintf(logbuf, "read10 %u, %u, %u, %u\r\n", lun, lba, offset, bufsize);
    cdc_send(0, logbuf, strlen(logbuf));

    if (bufsize > 4096) {
        bufsize = 4096;
    }

    if (lba >= i2c_get_msc_block_count(lun)) {
        sprintf(logbuf, "read10 out of range: %u > %u\r\n", lba, i2c_get_msc_block_count(lun));
        cdc_send(0, logbuf, strlen(logbuf));
        return -1;
    }

    flush_uart();

    msc_packet_t packet = {0};
    packet.header.magic = msc_packet_magic;
    packet.header.command = MSC_CMD_READ;
    packet.header.payload_length = sizeof(msc_payload_t);
    packet.payload.lun = lun;
    packet.payload.lba = lba;
    packet.payload.offset = offset;
    packet.payload.length = bufsize;
    packet.header.payload_crc = calc_crc32((void*) &packet.payload, sizeof(msc_payload_t));
    uart_write_blocking(UART_ESP32, (uint8_t*) &packet, sizeof(packet));

    uart_read_blocking(UART_ESP32, msc_buffer, bufsize + sizeof(msc_response_header_t));

    /*sprintf(logbuf, "Response from ESP32\r\n");
    cdc_send(0, logbuf, strlen(logbuf));*/

    memcpy(buffer, msc_buffer + sizeof(msc_response_header_t), bufsize);

    return (int32_t) bufsize;
}

bool tud_msc_is_writable_cb(uint8_t lun) {
  (void) lun;
  return true;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    char logbuf[256] = {0};
    sprintf(logbuf, "write10 %u, %u, %u, %u\r\n", lun, lba, offset, bufsize);
    cdc_send(0, logbuf, strlen(logbuf));

    if (bufsize > 4096) {
        bufsize = 4096;
    }

    if (lba >= i2c_get_msc_block_count(lun)) {
        sprintf(logbuf, "write10 out of range: %u > %u\r\n", lba, i2c_get_msc_block_count(lun));
        cdc_send(0, logbuf, strlen(logbuf));
        return -1;
    }

    cdc_send(0, logbuf, strlen(logbuf));
    flush_uart();
    cdc_send(0, logbuf, strlen(logbuf));
    memset(msc_buffer, 0, sizeof(msc_packet_t));
    cdc_send(0, logbuf, strlen(logbuf));
    memcpy(&msc_buffer[sizeof(msc_packet_t)], buffer, bufsize);
    cdc_send(0, logbuf, strlen(logbuf));
    msc_packet_t* packet = (msc_packet_t*) msc_buffer;
    packet->header.magic = msc_packet_magic;
    packet->header.command = MSC_CMD_WRIT;
    packet->header.payload_length = sizeof(msc_payload_t) + bufsize;
    packet->payload.lun = lun;
    packet->payload.lba = lba;
    packet->payload.offset = offset;
    packet->payload.length = bufsize;
    packet->header.payload_crc = calc_crc32((void*) &(packet->payload), sizeof(msc_payload_t) + bufsize);
    cdc_send(0, logbuf, strlen(logbuf));

    uint32_t position = 0;
    while (position < (sizeof(msc_packet_t) + bufsize)) {
        uint32_t txSize = sizeof(msc_packet_t) + bufsize - position;
        if (txSize > 64) {
            txSize = 64;
        }
        uart_write_blocking(UART_ESP32, &msc_buffer[position], txSize);
        position += txSize;
        sleep_us(250);
    }

    uart_read_blocking(UART_ESP32, msc_buffer, sizeof(msc_response_header_t));

    if (packet->header.magic != msc_packet_magic) {
        sprintf(logbuf, "write10 response magic invalid\r\n");
        cdc_send(0, logbuf, strlen(logbuf));
        return -1;
    }

    if (packet->header.command != MSC_ANS_OKOK) {
        sprintf(logbuf, "write10 response not OK\r\n");
        cdc_send(0, logbuf, strlen(logbuf));
        return -1;
    }
    
    return (int32_t) bufsize;
}

// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize) {
  // read10 & write10 has their own callback and MUST not be handled here

  void const* response = NULL;
  int32_t resplen = 0;

  // most scsi handled is input
  bool in_xfer = true;

  switch (scsi_cmd[0]) {
    default:
      // Set Sense = Invalid Command Operation
      tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

      // negative means error -> tinyusb could stall and/or response with failed status
      resplen = -1;
    break;
  }

  // return resplen must not larger than bufsize
  if ( resplen > bufsize ) resplen = bufsize;

  if ( response && (resplen > 0) ) {
    if(in_xfer) {
      memcpy(buffer, response, (size_t) resplen);
    } else {
      // SCSI output
    }
  }

  return (int32_t) resplen;
}

