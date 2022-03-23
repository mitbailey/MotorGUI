#include "imgui/imgui.h"
#include "serEncoder.hpp"
#include <time.h>
#include <inttypes.h>

int main()
{
    ScrollBuf dbuf(2000);
    SerEncoder enc("/dev/ttyUSB0", &dbuf, true);
    int count = 100;
    while (dbuf.data.empty() && count--)
    {
        printf("Waiting for data buffer to fill... %d s remaining\r", count);
        fflush(stdout);
        sleep(1);
        printf("                                                          \r");
    }
    printf("\n");
    int ofst = dbuf.ofst;
    while (dbuf.ofst < dbuf.max_sz)
    {
        uint64_t ts = dbuf.tstamp[dbuf.ofst];
        uint8_t flag = dbuf.flags[dbuf.ofst];
        int val = dbuf.data[dbuf.ofst];
        if (ofst != dbuf.ofst)
        {   
            printf("%" PRIu64 " %02x %d\n", ts, flag, val);
            fflush(stdout);
            ofst = dbuf.ofst;
        }
        else
        {
            usleep(1000);
        }
    }
    return 0;
}