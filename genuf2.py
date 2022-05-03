#!/usr/bin/env python3

import sys, math, binascii

def generateBlock(blockNumber, totalBlocks, familyId, flags, address, data):
    if len(data) > 476:
        raise ValueError
    uf2 = bytes([])
    firstMagic = 0x0A324655
    uf2 += firstMagic.to_bytes(4, byteorder='little')
    secondMagic = 0x9E5D5157
    uf2 += secondMagic.to_bytes(4, byteorder='little')
    uf2 += flags.to_bytes(4, byteorder='little')
    uf2 += address.to_bytes(4, byteorder='little')
    uf2 += (256).to_bytes(4, byteorder='little')#len(data).to_bytes(4, byteorder='little')
    uf2 += blockNumber.to_bytes(4, byteorder='little')
    uf2 += totalBlocks.to_bytes(4, byteorder='little')
    uf2 += familyId.to_bytes(4, byteorder='little')
    uf2 += data
    if (len(data) < 476):
        uf2 += bytes([0] * (476 - len(data)))
    finalMagic = 0x0AB16F30
    uf2 += finalMagic.to_bytes(4, byteorder='little')
    return uf2

def flashPad(data):
    data += bytes([0] * (len(data) % (1 << 8)))
    return data


def genHeader(address, data):
    header  = address.to_bytes(4, byteorder='little')
    header += (0x100100E9).to_bytes(4, byteorder='little')
    header += len(data).to_bytes(4, byteorder='little')
    crc     = binascii.crc32(data)
    header += crc.to_bytes(4, byteorder='little')
    print("Flash address     : 0x{:08X}".format(address))
    print("Entrypoint address: 0x{:08X}".format(0x100100E9))
    print("Flash size        : 0x{:08X}".format(len(data)))
    print("Flash CRC         : 0x{:08X}".format(crc))
    return header

def main():
    flags             = 0x00002000 # Family ID present
    familyId          = 0xe48bff56 # RP2040    
    bootloaderAddress = 0x10000000
    headerAddress     = 0x10008000
    appAddress        = 0x10010000
    
    if len(sys.argv) != 4:
        print("Usage: {} bootloader_binary application_binary output_uf2")
        sys.exit(1)
    
    bootloaderFilename = sys.argv[1]
    appFilename = sys.argv[2]
    outputFilename = sys.argv[3]
    
    with open(bootloaderFilename, "rb") as bootloaderFile:
        bootloaderBin = bootloaderFile.read()
    
    with open(appFilename, "rb") as appFile:
        appBin = appFile.read()
    
    bootloaderBin = flashPad(bootloaderBin)
    appBin = flashPad(appBin)
    header = flashPad(genHeader(appAddress, appBin))
    
    outputUf2 = bytes([])
    blockNumber = 0
    
    # Apparently the RP2040 ROM bootloader doesn't like it when the UF2 contains separate sections, pad everything into one big continuous data stream
    bootloaderBin += bytes([0] * (headerAddress - bootloaderAddress - len(bootloaderBin)))
    header += bytes([0] * (appAddress - headerAddress - len(header)))
    
    totalBlocks = math.ceil(len(bootloaderBin)/256) + math.ceil(len(appBin)/256) + math.ceil(len(header)/256)
    
    position = 0
    while True:
        data = bootloaderBin[position:256 + position]
        if len(data) < 1:
            break
        outputUf2 += generateBlock(blockNumber, totalBlocks, familyId, flags, bootloaderAddress + position, data)
        position += 256
        blockNumber += 1
    
    position = 0
    while True:
        data = header[position:256 + position]
        if len(data) < 1:
            break
        outputUf2 += generateBlock(blockNumber, totalBlocks, familyId, flags, headerAddress + position, data)
        position += 256
        blockNumber += 1

    position = 0
    while True:
        data = appBin[position:256 + position]
        if len(data) < 1:
            break
        outputUf2 += generateBlock(blockNumber, totalBlocks, familyId, flags, appAddress + position, data)
        position += 256
        blockNumber += 1

    with open(outputFilename, "wb") as outputFile:
        outputFile.write(outputUf2)
    

if __name__ == "__main__":
    main()
