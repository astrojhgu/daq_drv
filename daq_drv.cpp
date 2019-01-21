#include <pcap.h>
#include <cassert>
#include <iostream>
#include <cstdint>
#include <cstring>
#include <vector>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <unistd.h>
#include <future>
#include <fftw3.h>
#include <fcntl.h>           /* For O_* constants */
#include <algorithm>
#include "daq_drv.hpp"

void unmap(std::complex<float>* p)
{
    munmap(p, BUF_SIZE*sizeof(std::complex<float>));
}



Daq::Daq(const char* name1)
:dev_name(name1),
fd(init_fd(name1)),
write_buf((std::complex<float>*)mmap(nullptr,BUF_SIZE*sizeof(std::complex<float>), 
    PROT_WRITE|PROT_READ, MAP_SHARED,fd, 0), unmap),
proc_buf((std::complex<float>*)mmap(nullptr,BUF_SIZE*sizeof(std::complex<float>), 
    PROT_WRITE|PROT_READ, MAP_SHARED,fd, BUF_SIZE*sizeof(std::complex<float>)), unmap),
buf_id(0),swap_buf(false),
task([this](){this->run();})
{
    memset(write_buf.get(), 0x0f, BUF_SIZE*sizeof(std::complex<float>));
    memset(proc_buf.get(), 0x0f, BUF_SIZE*sizeof(std::complex<float>));
}

int Daq::init_fd(const char* name){
    auto fd=shm_open(name, O_RDWR|O_CREAT, 0777);
    ftruncate(fd, BUF_SIZE*2*sizeof(std::complex<float>));
    return fd;
}

Daq::~Daq(){
    task.join();
    shm_unlink(dev_name);
}

void Daq::swap(size_t bid){
    {
        std::lock_guard<std::mutex> lk(mx_swap);
        if(swap_buf){
            std::swap(write_buf, proc_buf);
            swap_buf.store(false);
            std::cout<<"swapped "<<bid<<" "<<dev_name<<std::endl;
        }
        else{
            std::cout<<"skipped "<<bid<<" "<<dev_name<<std::endl;
        }
        buf_id=bid;
    }
    cv.notify_all();
}


std::tuple<std::complex<float>*, size_t> Daq::fetch(){
    size_t current_id=0;
    {
        std::lock_guard<std::mutex> lk (mx_swap);
        swap_buf.store(true);
        current_id=buf_id.load();
    }

    std::unique_lock<std::mutex> lk(mx_cv);
    cv.wait(lk, [&]() {return current_id!=buf_id.load();});
    std::cerr<<"fetched "<<buf_id.load()<<" from "<<dev_name<<std::endl;
    //return std::make_tuple(read_buf.get(), buf_id.load());
   // read_buf.swap(proc_buf);
    return std::make_tuple(proc_buf.get(), buf_id.load());
}

std::future<std::tuple<std::complex<float>*, size_t>> Daq::fetch_async(){
    return std::async(std::launch::async, [this](){    
                return this->fetch();                
                });
}

void Daq::run(){
    char error_buffer[1024];
    const u_char *packet;
    pcap_pkthdr header;
    //pcap_t* cap=pcap_open_live(this->dev_name, 9000, 1, 1, ebuf);
    pcap_t *cap = pcap_create(this->dev_name, error_buffer);
    if(cap==nullptr){
        std::cout<<error_buffer<<std::endl;
        exit(1);
    }
    pcap_set_rfmon(cap, 0);
    pcap_set_promisc(cap, 1);
    pcap_set_snaplen(cap, 9000);
    pcap_set_timeout(cap, 1);
    
    if((pcap_set_buffer_size(cap, 512*1024*1024))!=0)
    {
        std::cerr<<"err"<<std::endl;
        exit(1);
    }
    pcap_activate(cap);

    std::vector<std::complex<float>> buf(N_CH*CH_SPLIT);
    
    std::vector<std::complex<float>> buf_fft(N_CH*CH_SPLIT);

    int rank=1;
    int howmany=N_CH;
    int n[]={CH_SPLIT};
    int idist=CH_SPLIT;
    int odist=CH_SPLIT;
    int istrid=1;
    int ostrid=1;
    int* inembed=n;
    int* onembed=n;

    auto fft=fftwf_plan_many_dft(rank, n, howmany, reinterpret_cast<fftwf_complex*>(buf.data()),
     inembed, istrid, idist, reinterpret_cast<fftwf_complex*>(buf_fft.data()), onembed, ostrid, odist, 
     FFTW_FORWARD, FFTW_MEASURE);


    size_t old_stat_id=0;
    size_t old_id=0;
    long x=0;
    u_int64_t id=0;

    std::complex<float>* buf_ptr=nullptr;
    for(size_t i=0;;++i){

        while(1){
            packet=pcap_next(cap, &header);
            if(packet!=nullptr && header.len==PACKET_SIZE){
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
        memcpy(&id, packet+42,8);
        
        std::complex<int16_t>* pci16=(std::complex<int16_t>*)(packet+50);
        
        auto shift2=id%CH_SPLIT;
        
        //memcpy(buf.data()+(id%CH_SPLIT)*2*N_CH, packet+50, N_CH*4);
        auto pingpong_id=id/CH_SPLIT/N_CHUNKS;
        auto old_pingpong_id=old_id/CH_SPLIT/N_CHUNKS;

        
        if(pingpong_id!=old_pingpong_id){
            this->swap(pingpong_id);
        }
        
        int16_t flip_factor=shift2%2==0?1:-1;//for fft shift
        //int16_t flip_factor=1;
        for(int i=0;i<N_CH;++i)
        {
            buf[shift2+i*CH_SPLIT]=pci16[i]*flip_factor;
            //buf_ptr[shift1+shift2+i*CH_SPLIT]=pci16[i];
        }
      
        buf_ptr=this->write_buf.get();
        
        if(id/CH_SPLIT!=old_id/CH_SPLIT){
            auto shift1=((id/CH_SPLIT)%N_CHUNKS)*CH_SPLIT*N_CH;
            auto buf_fft=buf_ptr+shift1;            
            fftwf_execute_dft(fft, 
                reinterpret_cast<fftwf_complex*>(buf.data()),
                reinterpret_cast<fftwf_complex*>(buf_fft)
            );
        }


        if(i%100000==0){
            //std::cout<<id<<" "<< 100000./(id-old_stat_id)<<std::endl;    
            if(id-old_stat_id!=100000){
                std::cout<<"losting "<<id-old_stat_id-100000<<" packets"<<std::endl;
            }        
            old_stat_id=id;
        }
	    old_id=id;
    }
}

DaqPool::DaqPool(const std::vector<const char*>& names)
{
    for(auto i:names){
        pool.push_back(std::move(std::unique_ptr<Daq>(new Daq(i))));
    }
}

std::tuple<size_t, std::vector<std::complex<float>*>> DaqPool::fetch(){
    auto n=pool.size();
    std::vector<size_t> id_list(n);
    std::vector<std::complex<float>*> ptr_list(n);
    while(1){
        std::vector<std::future<std::tuple<std::complex<float>*,size_t>>> futures;
        for(auto& i:pool){
            futures.push_back(i->fetch_async());
        }
        for(size_t i=0;i<futures.size();++i){
            std::tie(ptr_list[i], id_list[i])=futures[i].get();
        }
        auto m1=std::max_element(id_list.begin(), id_list.end());
        auto m2=std::min_element(id_list.begin(), id_list.end());
        if(m1==m2){
            std::cout<<"fetched"<<std::endl;
            break;
        }else{
            std::cout<<"async"<<std::endl;
        }
    }
    return std::make_tuple(id_list.front(), ptr_list);
}
