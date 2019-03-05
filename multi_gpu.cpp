#include "daq_drv.hpp"
#include <iostream>
#include <vector>
#include <future>
#include <omp.h>
#include "gpu_corr.h"
#include "errchk.h"
using namespace std;

constexpr size_t N_RAW_CH = 2048 / 2;
constexpr size_t CH_SPLIT = 32;
constexpr size_t N_CH=N_RAW_CH*CH_SPLIT;
constexpr size_t N_CHUNKS = 65536 / CH_SPLIT * 4;
constexpr int NNODES=2;
constexpr int NBASELINES=NNODES*(NNODES+1)/2;


constexpr int INPUT_LENGTH=N_CH*N_CHUNKS*NNODES;
constexpr int INPUT_LENGTH_ONE=N_CH*N_CHUNKS;
constexpr int OUTPUT_LENGTH=NBASELINES*N_CH;

int main (int argc, char *argv[])
{
    if (argc != 3)
        {
            cerr << "Usage: " << argv[0] << " ifnames" << std::endl;
            return -1;
        }

    cuInit(0);
    cudaDeviceReset();


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

    std::complex<float>* input_data;
    std::complex<float>* output_data;

    std::complex<float>* host_output_data=nullptr;

    gpuErrchk(cudaMalloc((void**)&input_data, INPUT_LENGTH*sizeof(float)*2));
    gpuErrchk(cudaMalloc((void**)&output_data, OUTPUT_LENGTH*sizeof(float)*2));
    gpuErrchk(cudaMallocHost((void**)&host_output_data, OUTPUT_LENGTH*sizeof(float)*2));

    

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
            
            //vector<std::complex<float>> corr (N_CH);
            for(size_t i=0;i<NNODES;++i)
            {
                gpuErrchk(cudaMemcpy((void*)(input_data+INPUT_LENGTH_ONE*i), (void*)(std::get<1> (data)[i]), INPUT_LENGTH_ONE*sizeof(float)*2, cudaMemcpyHostToDevice));
                auto p = std::get<1> (data)[i];
                //std::cout<<p[CH_SPLIT/2]<<std::endl;
            }

            gpuErrchk(cudaMemset(output_data, 0, OUTPUT_LENGTH*sizeof(float)*2));

            correlate_c(input_data,output_data,N_CH,NNODES,N_CHUNKS);

            gpuErrchk(cudaMemcpy((void*)host_output_data, (void*)output_data, OUTPUT_LENGTH*sizeof(float)*2, cudaMemcpyDeviceToHost));
            
            gpuErrchk(cudaDeviceSynchronize());

            for(int i=0;i<CH_SPLIT;++i)
            {
                std::cout<<host_output_data[i]<<std::endl;
            }
        }
}
