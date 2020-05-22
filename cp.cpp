#include <liburing.h>

const int DEPTH = 2;

struct io_data {
    int read;
    off_t offset[2];
    size_t len;
    iovec iov;
};
