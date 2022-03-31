#include "SerEncoder.hpp"

static char *defaultPrefix = (char *) "encoder";

static inline uint64_t ts_now()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (ts.tv_nsec + ts.tv_sec * 1000000000LLU);
}

SerEncoder::SerEncoder(const char *name, bool StoreData, const char *dataFilePrefix, bool data18)
{
    if (name == NULL || name == nullptr)
        throw std::runtime_error("Serial device name NULL");
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
    tty.c_cc[VTIME] = 10;   // 1 seconds read timeout

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
    char *msg = (char *)"$0V\r\n";
    ssize_t wr = write(fd, msg, strlen(msg));
    if (wr != (ssize_t)strlen(msg))
    {
        throw std::runtime_error("Could not write to the device");
    }
    ssize_t rd = 0;
    this->data18 = data18;
    for (int i = 10; (i > 0) && (rd <= 0); i--)
    {
        rd = read(fd, buf, sizeof(buf) - 1);
    }
    if (rd == 0)
    {
        throw std::runtime_error("Could not read serial number");
    }
    printf("%s: Serial number %s\n", __func__, buf);
    if (dataFilePrefix != NULL && dataFilePrefix != nullptr)
        dFilePrefix = (char *)dataFilePrefix;
    else
        dFilePrefix = defaultPrefix;
    thr = std::thread(Acquisition, this);
    thr.detach();
    sleep(1);
}

SerEncoder::~SerEncoder()
{
    stop = true;
    if (fp != NULL)
    {
        fflush(fp);
        fclose(fp);
        fp = NULL;
    }
    int _fd = fd;
    fd = 0;
    close(_fd);
}

bool SerEncoder::hasData() const
{
    return (count > 0);
}

uint64_t SerEncoder::dataCount() const
{
    return count;
}

void SerEncoder::getData(uint64_t &ts, int &oval, SerEncoder_Flags &flag)
{
    ts = this->ts;
    flag.val = 0;
    flag.val |= val & 0x1; // trailing zero bit
    flag.val <<= 1;
    val >>= 1;
    flag.val |= val & 0x1; // parity bit
    flag.val <<= 1;
    val >>= 1;
    flag.val |= val & 0x1; // va decoder error
    flag.val <<= 1;
    val >>= 1;
    flag.val |= val & 0x1; // signal quality wdog
    flag.val <<= 1;
    val >>= 1;
    flag.val |= val & 0x1; // quadrature error
    flag.val <<= 1;
    val >>= 1;
    oval = data18 ? val & 0x3ffff : val & 0x7ffff; // data bits
    val >>= data18 ? 19 : 20;
    flag.val |= val & 0x1; // va decoder status
}

void SerEncoder::Acquisition(void *_in)
{
    SerEncoder *in = (SerEncoder *)_in;
    if (in->store)
    {
        char fname[128];
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        snprintf(fname, sizeof(fname), "./%s_%04d%02d%02d_%02d%02d%02d.txt", in->dFilePrefix, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        in->fp = fopen(fname, "w");
    }
    char buf[50];
    memset(buf, 0x0, sizeof(buf));
    ssize_t rd = 0, wr = 0;
    // 2. Write encoder settings
    char *msg;
    if (in->data18)
        msg = (char *)"$0L1240\r\n";
    else
        msg = (char *)"$0L1250\r\n";
    wr = 0;
    for (int i = 10; (i > 0) && (wr <= 0); i--)
    {
        wr = write(in->fd, msg, strlen(msg));
    }
    if (in->data18)
        msg = (char *)"$0L2240\r\n";
    else
        msg = (char *)"$0L2250\r\n";
    wr = 0;
    for (int i = 10; (i > 0) && (wr <= 0); i--)
    {
        wr = write(in->fd, msg, strlen(msg));
    }
    if (in->data18)
        msg = (char *)"$0L1240\r\n";
    else
        msg = (char *)"$0L1250\r\n";
    wr = 0;
    for (int i = 10; (i > 0) && (wr <= 0); i--)
    {
        wr = write(in->fd, msg, strlen(msg));
    }
    // 3. Wait a sec
    usleep(100000); // 10 ms
    // 4. Start readin'
    msg = (char *)"$0A00010\r\n";
    wr = 0;
    for (int i = 10; (i > 0) && (wr <= 0); i--)
    {
        wr = write(in->fd, msg, strlen(msg));
    }
    while (!in->stop)
    {
        // 1. Detect '*0R0'
        static uint64_t hdr[1];
        memset(hdr, 0, sizeof(hdr));
        // read 4 bytes first
        while (strlen((const char *)hdr) < 4)
        {
            int len = strlen((const char *)hdr);
            rd = read(in->fd, hdr + len, 4 - len);
            if (rd < 0 && in->fd > 0)
                break;
            else if (rd < 0)
                throw std::runtime_error("Could not read from serial on line " + std::to_string(__LINE__));
        }
        // afterward...
        while (strncasecmp((const char *)hdr, "*0R0", 4) != 0)
        {
            // 1. read 1 byte
            char c = 0;
            rd = read(in->fd, &c, 1);
            if (rd < 0 && in->fd > 0)
                break;
            else if (rd < 0)
                throw std::runtime_error("Could not read from serial on line " + std::to_string(__LINE__));
            else if (rd == 0)
                continue;
            // 2. successfully read, shift by 1
            hdr[0] >>= 8;                  // shift by 1 byte
            hdr[0] |= ((uint64_t)c) << 32; // place captured byte at 5th byte place ([32..39])
            hdr[0] &= 0x000000ffffffffff;  // zero everything [40..63]
            // hdr[0] = hdr[1];
            // hdr[1] = hdr[2];
            // hdr[2] = hdr[3];
            // hdr[3] = hdr[4];
            // hdr[4] = c;
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
            if (rd < 0 && in->fd > 0)
                break;
            else if (rd < 0)
                throw std::runtime_error("Could not read from serial on line " + std::to_string(__LINE__));
            buf[idx] = c;
            idx += rd;
            if (idx > (int)sizeof(buf) - 1)
                throw std::runtime_error("Buffer overflow on line " + std::to_string(__LINE__));
        } while (c != ',');
        in->val = atoi(buf);
        // read part 2
        // memset(buf, 0x0, sizeof(buf));
        // idx = 0;
        // do
        // {
        //     c = 0;
        //     rd = read(in->fd, &c, 1);
        //     if (rd < 0 && in->fd > 0)
        //         break;
        //     else if (rd < 0)
        //         throw std::runtime_error("Could not read from serial on line " + std::to_string(__LINE__));
        //     buf[idx] = c;
        //     idx += rd;
        //     if (idx > (int) sizeof(buf) - 1)
        //         throw std::runtime_error("Buffer overflow on line " + std::to_string(__LINE__));
        // } while (c != '\r');
        in->ts = ts_now();
        // printf("%" PRIu64 ",%d\n", in->ts, in->val);
        in->count++;
        if (in->fp != NULL)
            fprintf(in->fp, "%" PRIu64 ",%d\n", in->ts, in->val);
    }
}
