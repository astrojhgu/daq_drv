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
        std::vector<future<void> > futures;
        for(auto& i:daqs){
            futures.push_back(std::async(std::launch::async, [&](){i->fetch();}));
        }
        for(auto& i:futures){
            i.get();
        }
//        daqs[0]->fetch();
    }
    for(auto& i:daqs){
        delete i;
    }
}
