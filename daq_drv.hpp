#ifndef DAQ_DRV
#define DAQ_DRV
#include <complex>
#include <memory>
#include <cstring>
#include <atomic>
#include <sys/mman.h>
#include <sys/stat.h> /* For mode constants */
#include <unistd.h>
#include <fcntl.h> /* For O_* constants */
#include <thread>
#include <mutex>
#include <condition_variable>
#include <tuple>
#include <vector>
#include <future>
#include <fftw3.h>
#include "bufq.hpp"

// static constexpr size_t N_CH=2048/2;
// static constexpr size_t SPEC_LEN=N_CH*4;
static constexpr size_t ID_SIZE = 8;
static constexpr size_t HEAD_LEN = 42;
// static constexpr size_t CH_SPLIT=4;
// static constexpr size_t PACKET_SIZE=SPEC_LEN+ID_SIZE+HEAD_LEN;
// static constexpr size_t N_CHUNKS=65536;
// static constexpr size_t BUF_SIZE=N_CH*CH_SPLIT*N_CHUNKS;


struct MMBuf
{
    std::complex<float> *ptr;
    std::size_t size;
    std::size_t buf_id;
    MMBuf () = delete;
    MMBuf (const MMBuf &) = delete;
    MMBuf &operator= (const MMBuf &) = delete;
    MMBuf (std::complex<float> *ptr1, std::size_t size1);
    ~MMBuf ();
};


class Daq
{
  public:
    const char *dev_name;
    const size_t n_raw_ch;
    const size_t ch_split;
    const size_t n_chunks;
    const size_t spec_len;
    const size_t packet_size;
    const size_t buf_size;
    int fd;
    BufQ<MMBuf> bufq;

    std::thread task;

    int cpu_id;
    std::vector<std::complex<float>> _buf;
    std::vector<std::complex<float>> _buf_fft;
    int fft_len[1];
    //fftwf_plan fft;

  public:
    Daq (const char *name1, size_t n_raw_ch1, size_t ch_split1, size_t n_chunks1, int cpu_id1);
    ~Daq ();

    int init_fd (const char *name);
    std::vector<std::shared_ptr<MMBuf>> init_buf ();

  public:
    void run ();
    MMBuf *fetch ();
    std::future<MMBuf *> fetch_async ();
    void bind_cpu ();
    fftwf_plan init_fft();
};


class DaqPool
{
  public:
    std::vector<std::unique_ptr<Daq>> pool;

  public:
    DaqPool (const std::vector<const char *> &names,
             size_t n_ch,
             size_t ch_split,
             size_t n_chunks,
             const std::vector<int> &cpu_ids);

    std::tuple<size_t, std::vector<std::complex<float> *>> fetch ();
};

#endif
