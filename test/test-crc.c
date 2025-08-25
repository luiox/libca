#include <stdio.h>
#include "util/crc.h"

int main()
{
    uint8_t data[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
    uint32_t crc = crc32_ieee(data, sizeof(data));
    if(crc != 0xf5a6aa3a){
        printf("crc32_ieee test failed!\n");
    }
    else{
        printf("crc32_ieee test passed!\n");
    }

    printf("CRC32: %08x\n", crc);

    return 0;
}
