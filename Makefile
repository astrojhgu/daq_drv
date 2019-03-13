CXX=g++-8
TARGETS=single multi #multi_gpu

all: $(TARGETS)
INC=-I /usr/local/cuda/include
CUDA_LIB=-L /usr/local/cuda/lib64/ -lcuda -lcudart
LIBS=-lpcap -lfftw3f -lrt
LDFLAGS=$(LIBS) -pthread -fopenmp -O3
CXXFLAGS= -march=native -fopenmp -ffast-math -Wall -O3 $(INC)

daq_drv.o: daq_drv.cpp daq_drv.hpp bufq.hpp
	$(CXX) -c $< -g -o $@ $(CXXFLAGS)

#about how to compile .cu with g++
#https://devtalk.nvidia.com/default/topic/1014106/separate-compilation-of-cuda-code-into-library-for-use-with-existing-code-base/
gpu_corr.a: gpu_corr.cu
	nvcc -ccbin g++ -m64  -dc $< -O3 -o gpu_corr.o
	nvcc -ccbin g++ -m64 -dlink gpu_corr.o -o gpu_corr.link.o 
	nvcc -ccbin g++ -m64 -lib  gpu_corr.link.o gpu_corr.o -o gpu_corr.a

single.o: single.cpp daq_drv.hpp
	$(CXX) -c $< -g -o $@ $(CXXFLAGS)

multi.o: multi.cpp daq_drv.hpp
	$(CXX) -c $< -g -o $@ $(CXXFLAGS)


single: single.o daq_drv.o
	$(CXX) $^ -g -o $@ -O3  $(LDFLAGS)


multi: multi.o daq_drv.o
	$(CXX) $^ -g -o $@ -O3  $(LDFLAGS)

multi_gpu:multi_gpu.o daq_drv.o gpu_corr.a
	$(CXX) $^ -g -o $@ -O3 $(LDFLAGS) $(CUDA_LIB)

clean:
	rm -f $(TARGETS) *.o gpu_corr.a 
