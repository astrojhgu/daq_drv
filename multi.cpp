#include "daq_drv.hpp"
#include <iostream>
#include <vector>
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
    for(auto& i:daqs){
        delete i;
    }
}
