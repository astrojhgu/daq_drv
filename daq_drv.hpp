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
#include <vector>
#include <future>

//static constexpr size_t N_CH=2048/2;
//static constexpr size_t SPEC_LEN=N_CH*4;
static constexpr size_t ID_SIZE=8;
static constexpr size_t HEAD_LEN=42;
//static constexpr size_t CH_SPLIT=4;
//static constexpr size_t PACKET_SIZE=SPEC_LEN+ID_SIZE+HEAD_LEN;
//static constexpr size_t N_CHUNKS=65536;
//static constexpr size_t BUF_SIZE=N_CH*CH_SPLIT*N_CHUNKS;


class Daq{
public:
    const char* dev_name;
    const size_t n_raw_ch;
    const size_t ch_split;
    const size_t n_chunks;
    const size_t spec_len;
    const size_t packet_size;
    const size_t buf_size;
    std::function<void(std::complex<float>*)> unmapper;
    int fd;
    std::unique_ptr<std::complex<float>, std::function<void(std::complex<float>*)>> write_buf;
    std::unique_ptr<std::complex<float>, std::function<void(std::complex<float>*)>> proc_buf;
    std::atomic_size_t buf_id;
    std::atomic_bool swap_buf;
    std::thread task;
    std::mutex mx_cv, mx_swap;
    std::condition_variable cv;

public:    
    Daq(const char* name1, size_t n_raw_ch1, size_t ch_split1, size_t n_chunks1);
    ~Daq();

    int init_fd(const char* name);
public:
    void swap(size_t bid);
    void run();
    std::tuple<std::complex<float>*, size_t> fetch();
    std::future<std::tuple<std::complex<float>*, size_t>> fetch_async();
};


class DaqPool{
    std::vector<std::unique_ptr<Daq>> pool;
public:
    DaqPool(const std::vector<const char*>& names, size_t n_ch, size_t ch_split, size_t n_chunks);

    std::tuple<size_t, std::vector<std::complex<float>*>> fetch();
};

#endif
