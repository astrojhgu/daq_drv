#include "daq_drv.hpp"
#include <iostream>
#include <vector>
#include <future>
using namespace std;

int main(int argc, char* argv[])
{
    if(argc!=3){
        cerr<<"Usage: "<<argv[0]<<" ifnames"<<std::endl;
        return -1;
    }

    std::vector<Daq*> daqs;
    for(int i=1;i<argc;++i){
        std::cout<<argv[i]<<std::endl;
        daqs.push_back(new Daq(argv[i]));
    }
    std::cout<<"waiting..."<<std::endl;
    
    while(1){
        //std::this_thread::sleep_for(5s);
        std::vector<future<float>> futures;
        for(auto& i:daqs){
            futures.push_back(std::async(std::launch::async, [&](){    
                auto result= i->fetch();
                auto p=std::get<0>(result);
                float x=0;
                for(int j=0;j<N_CH*CH_SPLIT*N_CHUNKS;++j){
                    x+=std::sqrt(std::norm(p[j]));
                }

                return x;
                }));
        }
        for(auto& i:futures){
            std::cout<<i.get()<<std::endl;;
            
        }
//        daqs[0]->fetch();
    }
    for(auto& i:daqs){
        delete i;
    }
}
