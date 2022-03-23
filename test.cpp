#include "imgui/imgui.h"
#include "serEncoder.hpp"
#include <time.h>
#include <inttypes.h>

int main()
{
    ScrollBuf dbuf(2000);
    SerEncoder enc("/dev/ttyUSB0", &dbuf);
    for (int i = 0; i < 4000; i++)
    {
        uint64_t ts = dbuf.tstamp[dbuf.ofst];
        uint8_t flag = dbuf.flags[dbuf.ofst];
        int val = dbuf.data[dbuf.ofst];
        printf("%" PRIu64 " %02x %d\n", ts, flag, val);
        fflush(stdout);
    }
    return 0;
}