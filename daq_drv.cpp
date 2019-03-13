#include <pcap.h>
#include <cassert>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <vector>
#include <cmath>
#include <sys/mman.h>
#include <sys/stat.h> /* For mode constants */
#include <unistd.h>
#include <future>
#include <fftw3.h>
#include <fcntl.h> /* For O_* constants */
#include <algorithm>
#include <pthread.h>
#include <sched.h>
#include "daq_drv.hpp"

std::mutex fft_mx;
constexpr size_t qlen = 2;
constexpr uint64_t ID_MASK = (((uint64_t)1 << 42) - 1);

MMBuf::MMBuf (std::complex<float> *ptr1, std::size_t size1) : ptr (ptr1), size (size1), buf_id (0)
{
}

MMBuf::~MMBuf ()
{
    assert (0);
    std::cout << "dstr" << std::endl;
    munmap (ptr, size * sizeof (std::complex<float>));
}


Daq::Daq (const char *name1, size_t n_raw_ch1, size_t ch_split1, size_t n_chunks1, int cpu_id1)
: dev_name (name1), n_raw_ch (n_raw_ch1), ch_split (ch_split1), n_chunks (n_chunks1),
  spec_len (n_raw_ch * 4), packet_size (spec_len + ID_SIZE + HEAD_LEN),
  buf_size (n_raw_ch * ch_split * n_chunks), fd (init_fd (name1)), bufq (init_buf ()),
  task ([this]() { this->run (); }), cpu_id (cpu_id1)
{
}

std::vector<std::shared_ptr<MMBuf>> Daq::init_buf ()
{
    std::vector<std::shared_ptr<MMBuf>> buf;

    for (size_t i = 0; i < qlen; ++i)
        {
            auto p = std::make_shared<MMBuf> (
            (std::complex<float> *)mmap (nullptr, buf_size * sizeof (std::complex<float>), PROT_WRITE | PROT_READ,
                                         MAP_SHARED, fd, i * buf_size * sizeof (std::complex<float>)),
            buf_size);
            buf.push_back (p);
        }
    return buf;
}

int Daq::init_fd (const char *name)
{
    auto fd = shm_open (name, O_RDWR | O_CREAT, 0777);
    if (ftruncate (fd, buf_size * qlen * sizeof (std::complex<float>)) != 0)
        {
            assert (0);
            exit (-1);
        }
    return fd;
}

fftwf_plan Daq::init_fft ()
{
    std::lock_guard<std::mutex> lk (fft_mx);
    
    _buf.resize (n_raw_ch * ch_split);

    _buf_fft.resize (n_raw_ch * ch_split);

    int rank = 1;
    int howmany = n_raw_ch;
    int fft_len[] = { (int)ch_split };
    int idist = ch_split;
    int odist = ch_split;
    int istrid = 1;
    int ostrid = 1;
    int *inembed = fft_len;
    int *onembed = fft_len;
    auto fft =
    fftwf_plan_many_dft (rank, fft_len, howmany, reinterpret_cast<fftwf_complex *> (_buf.data ()),
                         inembed, istrid, idist, reinterpret_cast<fftwf_complex *> (_buf_fft.data ()),
                         onembed, ostrid, odist, FFTW_FORWARD, FFTW_ESTIMATE);
    /*
    auto fft = fftwf_plan_many_dft (rank, fft_len, howmany, reinterpret_cast<fftwf_complex *> (_buf.data ()), inembed,
                                istrid, idist, reinterpret_cast<fftwf_complex *> (_buf_fft.data ()),
                                onembed, ostrid, odist, FFTW_FORWARD, FFTW_MEASURE);
*/
    return fft;
}

Daq::~Daq ()
{
    task.join ();
    shm_unlink (dev_name);
}


void Daq::bind_cpu ()
{
    int cpu_id = this->cpu_id >= 0 ? this->cpu_id : sched_getcpu ();

    cpu_set_t cpu_set;
    CPU_ZERO (&cpu_set);
    CPU_SET (cpu_id, &cpu_set);
    auto this_tid = task.native_handle ();
    std::cerr << "Bind this thread to CPU " << cpu_id << std::endl;
    pthread_setaffinity_np (this_tid, sizeof (cpu_set_t), &cpu_set);
}


std::shared_ptr<MMBuf> Daq::fetch ()
{
    return bufq.fetch ();
}

std::future<std::shared_ptr<MMBuf>> Daq::fetch_async ()
{
    return std::async (std::launch::async, [this]() { return this->fetch (); });
}

void Daq::run ()
{
    bind_cpu ();
    auto fft = init_fft ();
    char error_buffer[1024];
    const u_char *packet;
    pcap_pkthdr header;
    // pcap_t* cap=pcap_open_live(this->dev_name, 9000, 1, 1, ebuf);
    pcap_t *cap = pcap_create (this->dev_name, error_buffer);
    if (cap == nullptr)
        {
            std::cout << error_buffer << std::endl;
            exit (1);
        }
    pcap_set_rfmon (cap, 0);
    pcap_set_promisc (cap, 1);
    pcap_set_snaplen (cap, 9000);
    pcap_set_timeout (cap, 1);

    if ((pcap_set_buffer_size (cap, 512 * 1024 * 1024)) != 0)
        {
            std::cerr << "err" << std::endl;
            exit (1);
        }
    pcap_activate (cap);


    size_t old_stat_id = 0;
    size_t old_id = 0;
    // long x=0;
    uint64_t id = 0;

    //size_t old_shift2 = 0;
    // buf_ptr = nullptr;
    std::ofstream ofs ("fft.txt");
    std::ofstream ofs_idx ("idx.txt");
    size_t packet_cnt = 0;

    std::vector<std::complex<float>> buf (n_raw_ch * ch_split);
    auto p = bufq.prepare_write_buf ().get ();
    std::complex<float> *buf_ptr = p->ptr;

    auto now=std::chrono::system_clock::now();
    size_t byte_count=0;
    for (;;)
        {
            while (1)
                {
                    packet = pcap_next (cap, &header);
                    if (packet != nullptr && header.len == packet_size)
                        {
                            break;
                        }
                }
            byte_count+=packet_size;
            ++packet_cnt;
            memcpy (&id, packet + 42, 8);
            id&=ID_MASK;

            std::complex<int16_t> *pci16 = (std::complex<int16_t> *)(packet + 50);

            auto shift2 = id % ch_split;

            // memcpy(buf.data()+(id%ch_split)*2*n_raw_ch, packet+50, n_raw_ch*4);


            int16_t flip_factor = shift2 % 2 == 0 ? 1 : -1; // for fft shift
            // int16_t flip_factor=1;
            for (size_t i = 0; i < n_raw_ch; ++i)
                {
		  //assert (pci16[i].real () == 1);
                  //  assert (pci16[i].imag () == 0);
                    buf[shift2 + i * ch_split] = pci16[i] * flip_factor;
                    // buf_ptr[shift1+shift2+i*ch_split]=pci16[i];
                }

            if (id / ch_split != old_id / ch_split)
                {
                    auto shift1 = ((id / ch_split) % n_chunks) * ch_split * n_raw_ch;
                    auto buf_fft = buf_ptr + shift1;
                    fftwf_execute_dft (fft, reinterpret_cast<fftwf_complex *> (buf.data ()),
                                       reinterpret_cast<fftwf_complex *> (buf_fft));
                }


            if (packet_cnt % 100000 == 0)
                {
                    // std::cout<<id<<" "<< 100000./(id-old_stat_id)<<std::endl;
                    if (id - old_stat_id != 100000)
                        {
                            std::cout << "losting " << id - old_stat_id - 100000 << " packets" << std::endl;
                        }
                    old_stat_id = id;
                }

            auto buf_id = id / ch_split / n_chunks;
            auto old_buf_id = old_id / ch_split / n_chunks;
            if (buf_id != old_buf_id)
                {
                    auto now1=std::chrono::system_clock::now();
                    std::chrono::duration<double> dur=now1-now;
                    auto dt=dur.count();
                    auto MBps=byte_count/1e6/dt;
                    std::cerr<<"dt="<<dt<<" rate= "<<MBps<<" MBps"<<std::endl;
                    now=now1;
                    byte_count=0;            
                    // std::cout<<buf_id<<std::endl;
                    p->buf_id = buf_id;
                    bufq.submit ();
                    p = bufq.prepare_write_buf ().get ();
                    buf_ptr = p->ptr;
                }
            old_id = id;
        }
}

DaqPool::DaqPool (const std::vector<const char *> &names,
                  size_t n_ch,
                  size_t ch_split,
                  size_t n_chunks,
                  const std::vector<int> &cpu_ids)
{
    assert (names.size () == cpu_ids.size ());
    for (size_t i = 0; i < names.size (); ++i)
        {
            pool.push_back (
            std::move (std::unique_ptr<Daq> (new Daq (names[i], n_ch, ch_split, n_chunks, cpu_ids[i]))));
        }
}

std::tuple<size_t, std::vector<std::complex<float> *>> DaqPool::fetch ()
{
    auto n = pool.size ();
    std::vector<std::shared_ptr<MMBuf>> result_list (n, nullptr);


    std::vector<std::future<std::shared_ptr<MMBuf>>> futures;
    for (auto &i : pool)
        {
            futures.push_back (i->fetch_async ());
        }

    for (size_t i = 0; i < n; ++i)
        {
            result_list[i] = futures[i].get ();
        }

    // exit(0);
    while (1)
        {
            // id_max = *std::max_element (result_list.begin (), result_list.end ());
            size_t id_max = 0;
            for (auto &i : result_list)
                {
                    id_max = std::max (i->buf_id, id_max);
                }

            std::vector<size_t> to_be_fetched;
            for (size_t i = 0; i < n; ++i)
                {
                    if (result_list[i]->buf_id != id_max)
                        {
                            futures[i] = pool[i]->fetch_async ();
                            to_be_fetched.push_back (i);
                        }
                }
            if (to_be_fetched.empty ())
                {
                    break;
                }
            std::cerr << "Async, fetching..." << std::endl;
            for (auto i : to_be_fetched)
                {
                    std::cerr << "fetching device " << i << "    " << pool[i]->dev_name << std::endl;
                    result_list[i] = futures[i].get ();
                }
        }

    std::vector<std::complex<float> *> buffers (n);
    for (size_t i = 0; i < n; ++i)
        {
            buffers[i] = result_list[i]->ptr;
        }
    return std::make_tuple (result_list[0]->buf_id, buffers);
}
