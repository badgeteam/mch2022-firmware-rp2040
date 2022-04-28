#!/usr/bin/env python3

import binascii

def main():
    loadAddress       = 0x10004000
    dataSize          = 0x00007e70
    entrypointAddress = 0x100040e9
    expectedCrc       = 0xF80C0435
    
    header = entrypointAddress.to_bytes(4, byteorder='little') + dataSize.to_bytes(4, byteorder='little')
    
    print(binascii.hexlify(header))
    
    crc = binascii.crc32(header).to_bytes(4, byteorder='little')
    header += crc
    
    with open("build/header.bin", "wb") as f:
        f.write(header)
    
    print("Load address:       0x{:08X}".format(loadAddress))
    print("Data size:          0x{:08X}".format(dataSize))
    print("Entrypoint address: 0x{:08X}".format(entrypointAddress))
    print("Checksum:           0x{:08X}".format(int.from_bytes(crc, byteorder='little', signed=False)))
    print("Checksum should:    0x{:08X}".format(expectedCrc))


main()
