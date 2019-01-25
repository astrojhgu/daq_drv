#include <cassert>
#include <iostream>
#include <complex>
#include <vector>
#include <cuda.h>
#include "errchk.h"
#include <cstdint>
constexpr int BINSIZE_I=4;
constexpr int BINSIZE_J=4;
constexpr int MAX_BLOCK_X=1024;



__global__ void correlate_gpu(float* pdataIn,float* pdataOut,int nch,int nnodes,int ntime, int ch_seg, int length_per_node)
{
  int i=blockIdx.x;
  int j=blockIdx.y;
  
  if(j<i)
  {
    return;
  }
  int ch_seg_id=blockIdx.z;
  int ch_shift=ch_seg*ch_seg_id;
  int ch_id=ch_shift+threadIdx.x;

  if(ch_id>=nch)
  {
    return;
  }



  float cr=0;
  float ci=0;
  float r1,r2,i1,i2;
  cr=0;
  ci=0;
  
  for(int m=0;m<ntime;++m)
    {
       r1 = pdataIn[i*length_per_node+m*nch*2+2*ch_id];
       i1 = pdataIn[i*length_per_node+m*nch*2+2*ch_id+1];
       r2 = pdataIn[j*length_per_node+m*nch*2+2*ch_id];
       i2 = pdataIn[j*length_per_node+m*nch*2+2*ch_id+1];

      cr+=r1*r2+i1*i2;
      if(i!=j)
	    {
	      ci+=i1*r2-r1*i2;
	    }
    }

  int bl=(2*nnodes-1-i)*i/2+j;
  pdataOut[bl*2*nch+2*ch_id]+=cr/ntime;
  pdataOut[bl*2*nch+2*ch_id+1]+=ci/ntime;
}

extern "C" void correlate_c(std::complex<float>* input_data,std::complex<float>* output_data,int nch,int nnodes,int ntime)
{
  int by=(nch-1)/MAX_BLOCK_X+1;
  int ch_seg=nch/by;
  dim3 gd(nnodes,nnodes,by);
  int bx=std::min(nch, MAX_BLOCK_X);
  //std::cout<<bx<<" "<<by<<std::endl;
  dim3 bd(bx,1,1);
  int length_per_node=nch*ntime;

  correlate_gpu<<<gd, bd>>>((float*)input_data, (float*)output_data, nch,nnodes,ntime, ch_seg, length_per_node); 

}

#if 0

int main(int argc, char* argv[]){
    cuInit(0);
    cudaDeviceReset();

    int nch=8192;
    int ntime=65536/2;
    int nnodes=2;

    
    int nbaselines=nnodes*(nnodes+1)/2;


    int input_length=nch*ntime*nnodes;
    int output_length=nbaselines*nch;

    std::cout<<input_length<<std::endl;

    std::complex<float>* host_input_data=nullptr;
    std::complex<float>* host_output_data=nullptr;

    gpuErrchk(cudaMallocHost((void**)&host_input_data, input_length*sizeof(float)*2));
    gpuErrchk(cudaMallocHost((void**)&host_output_data, output_length*sizeof(float)*2));


    for(int i=0;i<input_length;++i){
        host_input_data[i]=std::complex<float>(1,1);
    }

    std::complex<float>* input_data;
    std::complex<float>* output_data;

    gpuErrchk(cudaMalloc((void**)&input_data, input_length*sizeof(float)*2));
    gpuErrchk(cudaMalloc((void**)&output_data, output_length*sizeof(float)*2));

    
    for(int i=0;i<100;++i){
        std::cout<<i<<std::endl;
        std::cout<<"beg"<<std::endl;
        gpuErrchk(cudaMemcpy((void*)input_data, (void*)host_input_data, input_length*sizeof(float)*2, cudaMemcpyHostToDevice));

        cudaMemset(output_data, 0, output_length*sizeof(float)*2);
        correlate_c(input_data,output_data,nch,nnodes,ntime);

        gpuErrchk(cudaMemcpy((void*)host_output_data, (void*)output_data, output_length*sizeof(float)*2, cudaMemcpyDeviceToHost));
        std::cout<<host_output_data[(nbaselines-1)*nch+nch-1]<<std::endl;
        gpuErrchk(cudaDeviceSynchronize());          
        
    }

    std::cout<<"Hello"<<std::endl;
}

#endif