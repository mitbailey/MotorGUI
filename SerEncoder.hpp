#ifndef _SER_ENCODER_HPP
#define _SER_ENCODER_HPP
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <signal.h>

#include <string>
#include <stdexcept>
#include <chrono>
#include <thread>

#include "imgui/imgui.h"

static inline uint64_t get_ts()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

class SerEncoder
{
public:
    SerEncoder(const char *name = (const char *) "/dev/ttyUSB0", bool StoreData = false);
    ~SerEncoder();
    bool hasData() const;
    uint64_t dataCount() const;
    void getData(uint64_t &ts, int &val, uint8_t &flag);
    static void Acquisition(void *_in);

private:
    int fd;
    bool stop;
    std::thread thr;
    bool store;
    FILE *fp;
    uint64_t ts;
    int val;
    uint64_t count;
};

#endif