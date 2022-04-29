#!/usr/bin/env python3

import binascii
from elftools.elf.elffile import ELFFile

def inFlash(addr, size):
    FlashBase = 0x10000000
    FlashSize = (2 * 1024 * 1024)
    return (addr >= FlashBase) and (addr + size <= FlashBase + FlashSize)

def main():
    entrypointAddress = 0x0
    flashStartAddress = 0x0
    rawFirmware = bytes([])
    dataSection = bytes([])
    FLASH_PAGE_SIZE = (1 << 8)
    with open("build/mch2022_firmware.elf", "rb") as f:
        elf = ELFFile(f)
        entrypointAddress = elf.header['e_entry']
        currentAddress = 0x0
        for index in range(elf.num_sections()):
            section = elf.get_section(index)
            addr = section.header["sh_addr"]
            size = section.header["sh_size"]
            if not inFlash(addr, size):
                if section.name == ".data": # Needed to initialize RAM
                    dataSection = section.data()
                continue
            if (len(rawFirmware) == 0):
                flashStartAddress = addr
                currentAddress = addr
            elif addr != currentAddress:
                print("Warning: segments not aligned", addr, currentAddress)
            
            rawFirmware += section.data()
            currentAddress += size
            #print(index, section.name, section.header["sh_type"], hex(addr), hex(size), hex(addr + size))
    rawFirmware += dataSection
    padding = len(rawFirmware) % FLASH_PAGE_SIZE
    rawFirmware += bytes([0] * padding)
    imageSize = len(rawFirmware)
    header  = flashStartAddress.to_bytes(4, byteorder='little')
    header += entrypointAddress.to_bytes(4, byteorder='little')
    header += imageSize.to_bytes(4, byteorder='little')
    crc     = binascii.crc32(rawFirmware)
    header += crc.to_bytes(4, byteorder='little')
    print("Flash address     : 0x{:08X}".format(flashStartAddress))
    print("Entrypoint address: 0x{:08X}".format(entrypointAddress))
    print("Flash size        : 0x{:08X}".format(imageSize))
    print("Flash CRC         : 0x{:08X}".format(crc))
    
    with open("build/header.bin", "wb") as f:
        f.write(header)
    with open("build/firmware.bin", "wb") as f:
        f.write(rawFirmware)

    print("Image header:       {}".format(binascii.hexlify(header)))

main()
