#include "daq_drv.hpp"
#include <iostream>
#include <vector>
#include <future>
#include <omp.h>
#include <fstream>

using namespace std;

constexpr size_t N_RAW_CH = 2048 / 2;
constexpr size_t CH_SPLIT = 1;
constexpr size_t N_CH=N_RAW_CH*CH_SPLIT;
constexpr size_t N_CHUNKS = 65536 / CH_SPLIT * 4;
constexpr double dnu=250.0/2048.0/CH_SPLIT;
int main (int argc, char *argv[])
{
    if (argc != 2)
        {
            cerr << "Usage: " << argv[0] << " ifname" << std::endl;
            return -1;
        }
    omp_set_dynamic (0);
    std::vector<const char *> names;
    std::vector<int> cpu_ids;
    int cpu_id = 0;
    for (int i = 1; i < argc; ++i)
        {
            names.push_back (argv[i]);
            // cpu_ids.push_back ((cpu_id + 8) % 16);
            cpu_ids.push_back (-1);
            cpu_id += 1;
        }

    DaqPool pool (names, N_RAW_CH, CH_SPLIT, N_CHUNKS, cpu_ids);
    size_t old_id=0;
    while (1)
        {
            //std::this_thread::sleep_for(std::chrono::seconds(3));
            // std::this_thread::sleep_for(5s);
            auto data = pool.fetch ();
            auto id=std::get<0> (data);
            std::cout << "fetched " << id <<" skipped "<<id-old_id-1<< std::endl;
            old_id=id;
	    int raw_ch_beg=std::get<2>(data);
	    int raw_ch_end=std::get<3>(data);
	    double freq_min=250/2048.*raw_ch_beg-dnu*CH_SPLIT/2;
            
            // continue;
            /*
            for(auto& i: std::get<1>(data)){
                std::cout<<i<<" ";
            }
            std::cout<<std::endl;
            */
            vector<std::complex<float>> corr (N_CH);
            auto ptr = std::get<1> (data)[0];

            
            for (size_t i = 0; i < N_CHUNKS; ++i)
                {
#pragma omp parallel for num_threads(4)
                    //#pragma omp parallel for num_threads(6)
                    for (size_t j = 0; j < N_CH; ++j)
                        {
			  
			  corr[j] += ptr[i * N_CH + j] *
                                       std::conj (ptr[i * N_CH + j]) / (float)N_CHUNKS;
			  
			  //corr[j]+=ptr[i*N_CH+j].real()/(float)N_CHUNKS;
                        }
                    
                }

	    ofstream ofs("auto.txt");

            //std::cout<<p1[CH_SPLIT/2+CH_SPLIT]<<std::endl;;
            std::cout << corr[CH_SPLIT / 2] << std::endl;

	    for (size_t i=0;i<N_CH;++i){
	      ofs<<freq_min+i*dnu<<" "<<corr[i].real()<<" "<<corr[i].imag()<<std::endl;
	    }
	    ofs.close();
        }
}
