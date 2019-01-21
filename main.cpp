#include "daq_drv.hpp"
#include <iostream>

using namespace std;

int main (int argc, char *argv[])
{
    if (argc != 2)
        {
            cerr << "Usage: " << argv[0] << " ifname" << std::endl;
            exit (0);
        }

    Daq daq (argv[1], 1024, 4, 65536);
}
