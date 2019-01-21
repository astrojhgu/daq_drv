#include <pcap.h>
#include <cassert>
#include <iostream>
#include <cstdint>
#include <cstring>
#include <vector>
#include <sys/mman.h>
#include <sys/stat.h> /* For mode constants */
#include <unistd.h>
#include <future>
#include <fftw3.h>
#include <fcntl.h> /* For O_* constants */
#include <algorithm>
#include <pthread.h>
#include "daq_drv.hpp"


Daq::Daq (const char *name1, size_t n_raw_ch1, size_t ch_split1, size_t n_chunks1)
: dev_name (name1), n_raw_ch (n_raw_ch1), ch_split (ch_split1), n_chunks (n_chunks1),
  spec_len (n_raw_ch * 4), packet_size (spec_len + ID_SIZE + HEAD_LEN),
  buf_size (n_raw_ch * ch_split * n_chunks),
  unmapper ([=](std::complex<float> *p) { munmap (p, buf_size * sizeof (std::complex<float>)); }),
  fd (init_fd (name1)),
  write_buf ((std::complex<float> *)
             mmap (nullptr, buf_size * sizeof (std::complex<float>), PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0),
             unmapper),
  proc_buf ((std::complex<float> *)mmap (nullptr,
                                         buf_size * sizeof (std::complex<float>),
                                         PROT_WRITE | PROT_READ,
                                         MAP_SHARED,
                                         fd,
                                         buf_size * sizeof (std::complex<float>)),
            unmapper),
  buf_id (0), swap_buf (false), task ([this]() { this->run (); })
{

    memset (write_buf.get (), 0x0f, buf_size * sizeof (std::complex<float>));
    memset (proc_buf.get (), 0x0f, buf_size * sizeof (std::complex<float>));
}

int Daq::init_fd (const char *name)
{
    auto fd = shm_open (name, O_RDWR | O_CREAT, 0777);
    if (ftruncate (fd, buf_size * 2 * sizeof (std::complex<float>)) != 0)
        {
            assert (0);
            exit (-1);
        }
    return fd;
}

Daq::~Daq ()
{
    task.join ();
    shm_unlink (dev_name);
}

void Daq::swap (size_t bid)
{
    {
        std::lock_guard<std::mutex> lk (mx_swap);
        if (swap_buf)
            {
                std::swap (write_buf, proc_buf);
                swap_buf.store (false);
                std::cout << "swapped " << bid << " " << dev_name << std::endl;
            }
        else
            {
                std::cout << "skipped " << bid << " " << dev_name << std::endl;
            }
        buf_id = bid;
    }
    cv.notify_all ();
}


std::tuple<std::complex<float> *, size_t> Daq::fetch ()
{
    size_t current_id = 0;
    {
        std::lock_guard<std::mutex> lk (mx_swap);
        swap_buf.store (true);
        current_id = buf_id.load ();
    }

    std::unique_lock<std::mutex> lk (mx_cv);
    cv.wait (lk, [&]() { return current_id != buf_id.load (); });
    std::cerr << "fetched " << buf_id.load () << " from " << dev_name << std::endl;
    // return std::make_tuple(read_buf.get(), buf_id.load());
    // read_buf.swap(proc_buf);
    return std::make_tuple (proc_buf.get (), buf_id.load ());
}

std::future<std::tuple<std::complex<float> *, size_t>> Daq::fetch_async ()
{
    return std::async (std::launch::async, [this]() { return this->fetch (); });
}

void Daq::run ()
{
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

    std::vector<std::complex<float>> buf (n_raw_ch * ch_split);

    std::vector<std::complex<float>> buf_fft (n_raw_ch * ch_split);

    int rank = 1;
    int howmany = n_raw_ch;
    int n[] = { (int)ch_split };
    int idist = ch_split;
    int odist = ch_split;
    int istrid = 1;
    int ostrid = 1;
    int *inembed = n;
    int *onembed = n;

    auto fft = fftwf_plan_many_dft (rank, n, howmany, reinterpret_cast<fftwf_complex *> (buf.data ()), inembed,
                                    istrid, idist, reinterpret_cast<fftwf_complex *> (buf_fft.data ()),
                                    onembed, ostrid, odist, FFTW_FORWARD, FFTW_MEASURE);


    size_t old_stat_id = 0;
    size_t old_id = 0;
    // long x=0;
    u_int64_t id = 0;

    std::complex<float> *buf_ptr = nullptr;
    for (size_t i = 0;; ++i)
        {

            while (1)
                {
                    packet = pcap_next (cap, &header);
                    if (packet != nullptr && header.len == packet_size)
                        {
                            break;
                        }
                }

            /*
            for(int i=0;i<PACKET_SIZE;++i)
            {
                std::cout<<std::hex<<(int)packet[i]<<" ";
            }
            return ;
            */
            memcpy (&id, packet + 42, 8);

            std::complex<int16_t> *pci16 = (std::complex<int16_t> *)(packet + 50);

            auto shift2 = id % ch_split;

            // memcpy(buf.data()+(id%ch_split)*2*n_raw_ch, packet+50, n_raw_ch*4);
            auto pingpong_id = id / ch_split / n_chunks;
            auto old_pingpong_id = old_id / ch_split / n_chunks;


            if (pingpong_id != old_pingpong_id)
                {
                    this->swap (pingpong_id);
                }

            int16_t flip_factor = shift2 % 2 == 0 ? 1 : -1; // for fft shift
            // int16_t flip_factor=1;
            for (size_t i = 0; i < n_raw_ch; ++i)
                {
                    buf[shift2 + i * ch_split] = pci16[i] * flip_factor;
                    // buf_ptr[shift1+shift2+i*ch_split]=pci16[i];
                }

            buf_ptr = this->write_buf.get ();

            if (id / ch_split != old_id / ch_split)
                {
                    auto shift1 = ((id / ch_split) % n_chunks) * ch_split * n_raw_ch;
                    auto buf_fft = buf_ptr + shift1;
                    fftwf_execute_dft (fft, reinterpret_cast<fftwf_complex *> (buf.data ()),
                                       reinterpret_cast<fftwf_complex *> (buf_fft));
                }


            if (i % 100000 == 0)
                {
                    // std::cout<<id<<" "<< 100000./(id-old_stat_id)<<std::endl;
                    if (id - old_stat_id != 100000)
                        {
                            std::cout << "losting " << id - old_stat_id - 100000 << " packets" << std::endl;
                        }
                    old_stat_id = id;
                }
            old_id = id;
        }
}

DaqPool::DaqPool (const std::vector<const char *> &names, size_t n_ch, size_t ch_split, size_t n_chunks)
{
    for (auto i : names)
        {
            pool.push_back (std::move (std::unique_ptr<Daq> (new Daq (i, n_ch, ch_split, n_chunks))));
        }
}

std::tuple<size_t, std::vector<std::complex<float> *>> DaqPool::fetch ()
{
    auto n = pool.size ();
    std::vector<size_t> id_list (n, 0);
    std::vector<std::complex<float> *> ptr_list (n);

    std::vector<std::future<std::tuple<std::complex<float> *, size_t>>> futures;
    for (auto &i : pool)
        {
            futures.push_back (i->fetch_async ());
        }
    for (size_t i = 0; i < n; ++i)
        {
            std::tie (ptr_list[i], id_list[i]) = futures[i].get ();
        }
    size_t id_max = 0;
    while (1)
        {
            id_max = *std::max_element (id_list.begin (), id_list.end ());

            std::vector<size_t> to_be_fetched;
            for (size_t i = 0; i < n; ++i)
                {
                    if (id_list[i] != id_max)
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
                    std::cerr << "fetching: " << i << std::endl;
                    std::tie (ptr_list[i], id_list[i]) = futures[i].get ();
                }
        }

    return std::make_tuple (id_max, ptr_list);
}
