#ifndef GPU_CORR_H
#define GPU_CORR_H
#include <complex>


extern "C" void correlate_c(std::complex<float>* input_data,std::complex<float>* output_data,int nch,int nnodes,int ntime);


#endif
