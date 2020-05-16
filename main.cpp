#include <iostream>
#include <liburing.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <memory>

using std::cerr, std::cout, std::ios, std::endl;

struct file_info {
    off_t sz;
    iovec iovecs[128];

    ~file_info() {
        for (int i = 0; iovecs[i].iov_base; ++i) {
            free(iovecs[i].iov_base);
        }
    }
};

off_t get_file_size(int fd) {
    static struct stat st;
    int r;
    if ((r = fstat(fd, &st))) {
        cerr << "Cannot stat file\n";
        return r;
    }
    if (S_ISBLK(st.st_mode)) { // block device?
        __u64 bytes;
        if ((r = ioctl(fd, BLKGETSIZE64, &bytes))) {
            cerr << "Cannot ioctl\n";
            return r;
        }
        return bytes;
    }
    if (S_ISREG(st.st_mode)) // regular file?
        return st.st_size;
    return 0;
}

int submit_read_request(char *file_path, io_uring &ring) {
    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        cerr << "Error opening file\n";
        return fd;
    }
    int current_block = 0;
    const off_t file_sz = get_file_size(fd);
    off_t remaining = file_sz;
    off_t offset = 0;
    int blocks = (file_sz + BLKGETSIZE - 1) / BLOCK_SIZE;
    auto *pInfo(new file_info);
    while (remaining) {
        off_t to_read = std::min(remaining, 1L * BLOCK_SIZE);
        offset += to_read;
        pInfo->iovecs[current_block].iov_len = to_read;
        void *buf;
        if (posix_memalign(&buf, BLOCK_SIZE, BLOCK_SIZE)) {
            cerr << "memalign error\n";
            return 1;
        }
        pInfo->iovecs[current_block].iov_base = buf;
        current_block++;
        remaining -= to_read;
    }
    pInfo->sz = file_sz;
    auto *sqe = io_uring_get_sqe(&ring);
    io_uring_prep_readv(sqe, fd, pInfo->iovecs, blocks, 0);
    io_uring_sqe_set_data(sqe, pInfo);
    io_uring_submit(&ring);

    return 0;
}

void console_output(const iovec &iovec) {
    for (int i = 0; i < iovec.iov_len; ++i) {
        cout << (static_cast<char *>(iovec.iov_base))[i];
    }
}

int process_completion(io_uring &ring) {
    io_uring_cqe *cqe;
    if (io_uring_wait_cqe(&ring, &cqe) < 0) {
        cerr << "io_uring_wailt error\n";
        return 1;
    }
    if (cqe->res < 0) {
        cerr << "Async readv failed\n";
        return 1;
    }
    std::unique_ptr<file_info> pInfo(static_cast<file_info *>(io_uring_cqe_get_data(cqe)));
    const off_t file_sz = pInfo->sz;
    int blocks = (file_sz + BLKGETSIZE - 1) / BLOCK_SIZE;
    for (int i = 0; i < blocks; ++i)
        console_output(pInfo->iovecs[i]);
    io_uring_cqe_seen(&ring, cqe);
    return 0;
}

int main(int argc, char *argv[]) {
    const int QUEUE_DEPTH = 1;
    io_uring ring{};
    if (argc < 2) {
        cerr << "Missing argument\n";
        return 1;
    }

    io_uring_queue_init(QUEUE_DEPTH, &ring, 0);

    for (int i = 1; i < argc; ++i) {
        const int r = submit_read_request(argv[i], ring);
        if (r) {
            cerr << "Error reading file\n";
            return r;
        }
        process_completion(ring);
    }

    io_uring_queue_exit(&ring);
}
