CXX=clang++

all: run multi
run: main.cpp daq_drv.cpp
	$(CXX) main.cpp daq_drv.cpp -o run -O3 -lpcap -lfftw3f -lrt -march=native -pthread

multi: multi.cpp daq_drv.cpp
	$(CXX) $^ -o multi -O3 -lpcap -lfftw3f -lrt -march=native -pthread

clean:
	rm -f run multi
