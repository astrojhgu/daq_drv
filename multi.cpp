#include "daq_drv.hpp"
#include <iostream>
#include <vector>
#include <future>
#include <omp.h>
using namespace std;

constexpr size_t N_CH = 2048 / 2;
constexpr size_t CH_SPLIT = 8;
constexpr size_t N_CHUNKS = 65536 / CH_SPLIT * 4;

int main (int argc, char *argv[])
{
    if (argc != 3)
        {
            cerr << "Usage: " << argv[0] << " ifnames" << std::endl;
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

    DaqPool pool (names, N_CH, CH_SPLIT, N_CHUNKS, cpu_ids);

    while (1)
        {
            // std::this_thread::sleep_for(5s);
            auto data = pool.fetch ();
            std::cout << "fetched " << std::get<0> (data) << std::endl;
            ;
            /*
            for(auto& i: std::get<1>(data)){
                std::cout<<i<<" ";
            }
            std::cout<<std::endl;
            */
            vector<std::complex<float>> corr (N_CH * CH_SPLIT);
            auto p1 = std::get<1> (data)[0];
            auto p2 = std::get<1> (data)[1];
            for (size_t i = 0; i < N_CHUNKS; ++i)
                {
#pragma omp parallel for num_threads(4)
                    //#pragma omp parallel for num_threads(6)
                    for (size_t j = 0; j < N_CH * CH_SPLIT; ++j)
                        {
                            corr[j] += p1[i * N_CH * CH_SPLIT + j] *
                                       std::conj (p2[i * N_CH * CH_SPLIT + j]) / (float)N_CHUNKS;
                        }
                }
            std::cout << corr[CH_SPLIT / 2] << std::endl;
            /*
            for(int j=0;j<N_CH*CH_SPLIT*N_CHUNKS;++j){
                        //x+=std::sqrt(std::norm(p[j]));
                corr[j%(N_CH*CH_SPLIT)]+=p1[j]*std::conj(p2[j]);
            }*/
        }
}
