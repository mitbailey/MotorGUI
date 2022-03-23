#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <string>

#include "imgui/imgui.h"

class ScrollBuf
{
public:
    ScrollBuf()
    {
        max_sz = 600;
        ofst = 0;
        tstamp.reserve(max_sz);
        flags.reserve(max_sz);        data.reserve(max_sz);
    }

    ScrollBuf::ScrollBuf(int max_size)
    {
        max_sz = max_size;
        ofst = 0;
        tstamp.reserve(max_sz);
        flags.reserve(max_sz);        data.reserve(max_sz);
    }

    void ScrollBuf::AddPoint(uint64_t ts, uint8_t flag, int d)
    {
        if (data.size() < max_sz)
        {
            data.push_back(d);
            tstamp.push_back(ts);
            flags.push_back(flag);
        }
        else
        {
            data[ofst] = d;
            tstamp[ofst] = ts;
            flags[ofst] = flag;

            ofst = (ofst + 1) % max_sz;
        }
    }
    void ScrollBuf::Erase()
    {
        if (data.size() > 0)
        {
            data.shrink(0);
            ofst = 0;
        }
    }
    double ScrollBuf::Max()
    {
        double max = data[0];
        for (int i = 0; i < data.size(); i++)
            if (data[i] > max)
                max = data[i];
        return max;
    }
    double ScrollBuf::Min()
    {
        double min = data[0];
        for (int i = 0; i < data.size(); i++)
            if (data[i] < min)
                min = data[i];
        return min;
    }

    int max_sz;
    int ofst;
    ImVector<uint64_t> tstamp;
    ImVector<uint8_t> flags;
    ImVector<int> data;
};

class SerEncoder
{
public:
    SerEncoder(const char *name)
    {
        if (name == NULL || name == nullptr)
            throw std::runtime_error("Serial device name NULL");
        fd = open(name, O_RDWR | O_NOCTTY | O_SYNC);
        if (fd < 0)
            throw std::runtime_error("Could not open device " + std::to_string(name));
        struct termios tty;
        if (tcgetattr(fd, &tty) != 0)
        {
            throw std::runtime_error("Error from tcgetattr: " + std::to_string(errno) + " " + std::to_string(strerror(errno)));
        }

        int speed = B115200;

        cfsetospeed(&tty, speed);
        cfsetispeed(&tty, speed);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars
        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK; // disable break processing
        tty.c_lflag = 0;        // no signaling chars, no echo,
                                // no canonical processing
        tty.c_oflag = 0;        // no remapping, no delays
        tty.c_cc[VMIN] = 0;     // read doesn't block
        tty.c_cc[VTIME] = 10;   // 10 seconds read timeout

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

        tty.c_cflag |= (CLOCAL | CREAD);   // ignore modem controls,
                                           // enable reading
        tty.c_cflag &= ~(PARENB | PARODD); // shut off parity
        tty.c_cflag |= parity;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS; // gnu99 compilation

        if (tcsetattr(fd, TCSANOW, &tty) != 0)
        {
             throw std::runtime_error("Error from tcsetattr: " + std::to_string(errno) + " " + std::to_string(strerror(errno)));
        }
        // start reading data here
        // 1. Get serial number
        char buf[50];
        memset(buf, 0x0, sizeof(buf));
        char *msg = "$0\r\n";
        if (write(fd, msg, strlen(msg)) != strlen(msg))
        {
            throw std::runtime_error("Could not write to the device");
        }
        ssize_t rd = 0;
        for (int i = 10; (i > 0) && (rd <= 0); i--)
        {
            rd = read(fd, buf, sizeof(buf) - 1);
        }
        if (rd == 0)
        {
            throw std::runtime_error("Could not read serial number");
        }
        printf("%s: Serial number %s\n", __func__, buf);
        // 2. Write encoder settings
        msg = "$0L1250\r\n";
        ssize_t wr = 0;
        for (int i = 10; (i > 0) && (wr <= 0); i--)
        {
            wr = write(fd, msg, strlen(msg));
        }
        msg = "$0L2250\r\n";
        ssize_t wr = 0;
        for (int i = 10; (i > 0) && (wr <= 0); i--)
        {
            wr = write(fd, msg, strlen(msg));
        }
        msg = "$0L1250\r\n";
        ssize_t wr = 0;
        for (int i = 10; (i > 0) && (wr <= 0); i--)
        {
            wr = write(fd, msg, strlen(msg));
        }
        // 3. Wait a sec
        usleep(100000); // 10 ms
        // 4. Start readin'
    }

public:
    ScrollBuf buf;

private:
    int fd;
    bool stop;
};