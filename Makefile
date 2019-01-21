CXX=g++

all: multi libdaq_drv.so
run: main.cpp daq_drv.cpp
	$(CXX) $^ -o $@ -O3 -lpcap -lfftw3f -lrt -march=native -pthread -fopenmp

multi: multi.cpp daq_drv.cpp
	$(CXX) $^ -g -o $@ -O3 -lpcap -lfftw3f -lrt -march=native -pthread -fopenmp -Wall

libdaq_drv.so: daq_drv.cpp
	$(CXX) -fPIC --shared -O3 $< -o $@

clean:
	rm -f run multi
