CXX=g++
TARGETS=single multi

all: $(TARGETS)
LIBS=-lpcap -lfftw3f -lrt
LDFLAGS=$(LIBS) -pthread -fopenmp -O3
CXXFLAGS=-march=native -fopenmp -ffast-math -Wall -O3

daq_drv.o: daq_drv.cpp
	$(CXX) -c $< -g -o $@ $(CXXFLAGS)

single.o: single.cpp
	$(CXX) -c $< -g -o $@ $(CXXFLAGS)

multi.o: multi.cpp
	$(CXX) -c $< -g -o $@ $(CXXFLAGS)


single: single.o daq_drv.o
	$(CXX) $^ -g -o $@ -O3  $(LDFLAGS)


multi: multi.o daq_drv.o
	$(CXX) $^ -g -o $@ -O3  $(LDFLAGS)

clean:
	rm -f $(TARGETS) *.o
