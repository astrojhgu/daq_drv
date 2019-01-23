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

    Daq daq (argv[1], 1024, 16, 65536 * 4 / 16, -1);
    while (1)
        {
            auto mmb = daq.fetch ();
            std::cout << " " << mmb->buf_id << " " << mmb->ptr[8] << std::endl;
        }
}
