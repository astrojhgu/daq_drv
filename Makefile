CXX=g++

all: run multi
run: main.cpp daq_drv.cpp
	$(CXX) main.cpp daq_drv.cpp -o run -O3 -lpcap -lfftw3f -lrt -march=native -pthread -fopenmp

multi: multi.cpp daq_drv.cpp
	$(CXX) $^ -g -o $@ -O3 -lpcap -lfftw3f -lrt -march=native -pthread -fopenmp -Wall -march=native

clean:
	rm -f run multi
