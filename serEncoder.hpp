#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#include <string>
#include <stdexcept>
#include <chrono>

#include "imgui/imgui.h"

class ScrollBuf
{
public:
    ScrollBuf()
    {
        max_sz = 600;
        ofst = 0;
        tstamp.reserve(max_sz);
        flags.reserve(max_sz);
        data.reserve(max_sz);
    }

    ScrollBuf(int max_size)
    {
        max_sz = max_size;
        ofst = 0;
        tstamp.reserve(max_sz);
        flags.reserve(max_sz);
        data.reserve(max_sz);
    }

    void AddPoint(uint64_t ts, uint8_t flag, int d)
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
    void Erase()
    {
        if (data.size() > 0)
        {
            data.shrink(0);
            ofst = 0;
        }
    }
    double Max()
    {
        double max = data[0];
        for (int i = 0; i < data.size(); i++)
            if (data[i] > max)
                max = data[i];
        return max;
    }
    double Min()
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

static inline uint64_t get_ts()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

class SerEncoder
{
public:
    SerEncoder(const char *name, ScrollBuf *dbuf)
    {
        if (name == NULL || name == nullptr)
            throw std::runtime_error("Serial device name NULL");
        if (dbuf == NULL || dbuf == nullptr)
            throw std::runtime_error("Scroll buffer NULL");
        this->dbuf = dbuf;
        fd = open(name, O_RDWR | O_NOCTTY | O_SYNC);
        if (fd < 0)
            throw std::runtime_error("Could not open device " + std::string(name));
        struct termios tty;
        if (tcgetattr(fd, &tty) != 0)
        {
            throw std::runtime_error("Error from tcgetattr: " + std::to_string(errno) + " " + std::string(strerror(errno)));
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
        tty.c_cflag |= 0;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS; // gnu99 compilation

        if (tcsetattr(fd, TCSANOW, &tty) != 0)
        {
            throw std::runtime_error("Error from tcsetattr: " + std::to_string(errno) + " " + std::string(strerror(errno)));
        }
        // start reading data here
        // 1. Get serial number
        char buf[50];
        memset(buf, 0x0, sizeof(buf));
        char *msg = "$0V\r\n";
        ssize_t wr = write(fd, msg, strlen(msg));
        if (wr != (ssize_t) strlen(msg))
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

        pthread_attr_t attr;
        int rc = pthread_attr_init(&attr);
        rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        printf("Creating acquisition thread on %p\n", this);
        stop = false;
        rc = pthread_create(&thr, NULL, &Acquisition, (void *)this);
        if (rc != 0)
        {
            throw std::runtime_error("Error creating data acquisition thread: " + std::to_string(errno) + " " + std::string(strerror(errno)));
        }
        pthread_attr_destroy(&attr);
        sleep(1);
    }

    ~SerEncoder()
    {
        stop = true;
        sleep(1);
        pthread_cancel(thr);
        close(fd);
    }

    static void *Acquisition(void *_in)
    {
        printf("Acquisition thread on object %p\n", _in);
        SerEncoder *in = (SerEncoder *) _in;
        char buf[50];
        memset(buf, 0x0, sizeof(buf));
        ssize_t rd = 0, wr = 0;
        // 2. Write encoder settings
        char *msg = "$0L1250\r\n";
        wr = 0;
        for (int i = 10; (i > 0) && (wr <= 0); i--)
        {
            wr = write(in->fd, msg, strlen(msg));
        }
        msg = "$0L2250\r\n";
        wr = 0;
        for (int i = 10; (i > 0) && (wr <= 0); i--)
        {
            wr = write(in->fd, msg, strlen(msg));
        }
        msg = "$0L1250\r\n";
        wr = 0;
        for (int i = 10; (i > 0) && (wr <= 0); i--)
        {
            wr = write(in->fd, msg, strlen(msg));
        }
        // 3. Wait a sec
        usleep(100000); // 10 ms
        // 4. Start readin'
        msg = "$0A00010\r\n";
        wr = 0;
        for (int i = 10; (i > 0) && (wr <= 0); i--)
        {
            wr = write(in->fd, msg, strlen(msg));
        }
        while (!in->stop)
        {
            // 1. Detect '*0R0'
            static char hdr[5];
            memset(hdr, 0, sizeof(hdr));
            // read 4 bytes first
            while (strlen(hdr) < 4)
                ;
            {
                int len = strlen(hdr);
                rd = read(in->fd, hdr + len, 4 - len);
                if (rd < 0)
                {
                    throw std::runtime_error("Could not read from serial on line " + std::to_string(__LINE__));
                }
            }
            // afterward...
            while (strncasecmp(hdr, "*0R0", 4) != 0)
            {
                // 1. read 1 byte
                char c = 0;
                rd = read(in->fd, &c, 1);
                if (rd < 0)
                {
                    throw std::runtime_error("Could not read from serial on line " + std::to_string(__LINE__));
                }
                else if (rd == 0)
                    continue;
                // 2. successfully read, shift by 1
                hdr[0] = hdr[1];
                hdr[1] = hdr[2];
                hdr[3] = hdr[4];
                hdr[4] = c;
            }
            // pattern matched at this point
            // read up to , for port 1; read after , to \r for port 2
            memset(buf, 0x0, sizeof(buf));
            static int idx;
            idx = 0;
            char c;
            do
            {
                c = 0;
                rd = read(in->fd, &c, 1);
                if (rd < 0)
                    throw std::runtime_error("Could not read from serial on line " + std::to_string(__LINE__));
                buf[idx] = c;
                idx += rd;
                if (idx > (int) sizeof(buf) - 1)
                    throw std::runtime_error("Buffer overflow on line " + std::to_string(__LINE__));
            } while (c != ',');
            int d1 = atoi(buf);
            // read part 2
            // memset(buf, 0x0, sizeof(buf));
            // idx = 0;
            // do
            // {
            //     c = 0;
            //     rd = read(in->fd, &c, 1);
            //     if (rd < 0)
            //         throw std::runtime_error("Could not read from serial on line " + std::to_string(__LINE__));
            //     buf[idx] = c;
            //     idx += rd;
            //     if (idx > (int) sizeof(buf) - 1)
            //         throw std::runtime_error("Buffer overflow on line " + std::to_string(__LINE__));
            // } while (c != '\r');
            static uint64_t ts;
            static uint8_t flag;
            static int val;
            ts = get_ts();
            flag |= (d1 >> 26);          // 25 bit readout, bit 0 is VA decoder status bit
            flag <<= 1;                  // move by 1 bit to make room for VA decoder status bit
            flag |= d1 & 0x1;            // VA Decoder status bit
            val = (d1 & 0x3fffffe) >> 1; // select [1..25]
            in->dbuf->AddPoint(ts, flag, val);
        }
        return NULL;
    }

private:
    ScrollBuf *dbuf;
    int fd;
    bool stop;
    pthread_t thr;
};