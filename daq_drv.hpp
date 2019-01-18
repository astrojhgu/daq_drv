#ifndef DAQ_DRV
#define DAQ_DRV
#include <complex>
#include <memory>
#include <cstring>
#include <atomic>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <unistd.h>
#include <fcntl.h>           /* For O_* constants */
#include <thread>
#include <mutex>
#include  <condition_variable>
#include <tuple>

static constexpr size_t N_CH=2048/2;
static constexpr size_t SPEC_LEN=N_CH*4;
static constexpr size_t ID_SIZE=8;
static constexpr size_t HEAD_LEN=42;
static constexpr size_t CH_SPLIT=4;
static constexpr size_t PACKET_SIZE=SPEC_LEN+ID_SIZE+HEAD_LEN;
static constexpr size_t N_CHUNKS=65536;
static constexpr size_t BUF_SIZE=N_CH*CH_SPLIT*N_CHUNKS;

void start_daq(const char* dev_name);

void unmap(std::complex<float>* p);

class Daq{
public:
    const char* dev_name;
    int fd;
    std::unique_ptr<std::complex<float>, decltype(&unmap)> write_buf;
    std::unique_ptr<std::complex<float>, decltype(&unmap)> read_buf;
    std::unique_ptr<std::complex<float>, decltype(&unmap)> proc_buf;
    std::atomic_size_t buf_id;
    std::thread task;
    std::mutex mx;
    std::condition_variable cv;

public:    
    Daq(const char* name1);
    ~Daq();

    int init_fd(const char* name);
public:
    void swap(size_t bid);
    void run();
    std::tuple<std::complex<float>*, size_t> fetch();
};



#endif
