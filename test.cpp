#include "imgui/imgui.h"
#include "serEncoder.hpp"
#include <time.h>
#include <inttypes.h>

int main()
{
    SerEncoder enc("/dev/ttyUSB0", true);
    int count = 100;
    while (!enc.hasData() && count--)
    {
        printf("Waiting for data buffer to fill... %d s remaining\r", count);
        fflush(stdout);
        sleep(1);
        printf("                                                          \r");
    }
    printf("\n");
    while (enc.dataCount() < 5000) // should take 5 seconds
    {
        uint64_t ts;
        uint8_t flag;
        int val;
        static uint64_t ct = enc.dataCount();
        if (ct != enc.dataCount())
        {   
            enc.getData(ts, val, flag);
            printf("%" PRIu64 " %02x %d\n", ts, flag, val);
            fflush(stdout);
            ct = enc.dataCount();
        }
        else
        {
            usleep(1000);
        }
    }
    return 0;
}