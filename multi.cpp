#include "daq_drv.hpp"
#include <iostream>
#include <vector>
#include <future>
#include <omp.h>
using namespace std;

int main(int argc, char* argv[])
{
    if(argc!=3){
        cerr<<"Usage: "<<argv[0]<<" ifnames"<<std::endl;
        return -1;
    }
    omp_set_dynamic(0); 
    std::vector<const char*> names;
    for(size_t i=1;i<argc;++i){
        names.push_back(argv[i]);
    }

    DaqPool pool(names);
    
    while(1){
        //std::this_thread::sleep_for(5s);
        auto data=pool.fetch();
        /*
        for(auto& i: std::get<1>(data)){
            std::cout<<i<<" ";
        }
        std::cout<<std::endl;
        */
        vector<std::complex<float>> corr(N_CH*CH_SPLIT);
        auto p1=std::get<1>(data)[0];
        auto p2=std::get<1>(data)[1];
        for(int i=0;i<N_CHUNKS;++i){
            #pragma omp parallel for num_threads(4)
            for(int j=0;j<N_CH*CH_SPLIT;++j){
                corr[j]+=p1[i*N_CH*CH_SPLIT+j]*p2[i*N_CH*CH_SPLIT+j];
            }
        }
        /*
        for(int j=0;j<N_CH*CH_SPLIT*N_CHUNKS;++j){
                    //x+=std::sqrt(std::norm(p[j]));
            corr[j%(N_CH*CH_SPLIT)]+=p1[j]*std::conj(p2[j]);
        }*/
    }
}
