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

typedef union
{
    struct __attribute__((packed))
    {
        uint8_t sta : 1;    // va decoder status
        uint8_t qe : 1;     // quadrature error
        uint8_t sqw : 1;    // signal quality watchdog
        uint8_t de : 1;     // va decoder error
        uint8_t parity : 1; // parity bit
        uint8_t tz : 1;     // trailing zero
        uint8_t unused : 2;
    };
    uint8_t val;
} SerEncoder_Flags;

class SerEncoder
{
public:
    SerEncoder(const char *name = (const char *)"/dev/ttyUSB0", bool StoreData = false, const char *dataFilePrefix = NULL, bool data18 = false);
    ~SerEncoder();
    bool hasData() const;
    uint64_t dataCount() const;
    void getData(uint64_t &ts, int &oval, SerEncoder_Flags &flag);
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
    bool data18;
    char *dFilePrefix;
};

#endif