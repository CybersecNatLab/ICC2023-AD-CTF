#!/usr/bin/env python3

import pefile
import sys


def main():
    if len(sys.argv) != 2:
        print(f'Usage: {sys.argv[0]} <bcryptprimitives.dll>', file=sys.stderr)
        exit(1)

    pe = pefile.PE(sys.argv[1])

    for exp in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        if exp.name == b'ProcessPrng':
            processprng_off = exp.address
            break
    else:
        raise Exception('could not find ProcessPrng export')

    processprng_lo12 =  processprng_off & 0xfff
    print(f'PROCESSPRNG_LO12 = 0x{processprng_lo12:03x}')
    print(f'PROCESSPRNG_OFF = 0x{processprng_off:x}')


if __name__ == '__main__':
    main()
