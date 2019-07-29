#include "daq_drv.hpp"
#include <iostream>
#include <vector>
#include <future>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <omp.h>
using namespace std;

constexpr size_t N_RAW_CH = 1680-368;
constexpr size_t CH_SPLIT = 1;
constexpr size_t N_CH = N_RAW_CH * CH_SPLIT;
constexpr size_t N_CHUNKS = 65536 / CH_SPLIT * 4;
constexpr double dnu = 250.0 / 2048.0 / CH_SPLIT;
constexpr double sq_mean_k_rate=1e-1;
constexpr double sq_mean_k_init=0.0;
constexpr double sq_mean_k_final=1.-1e-11;

static void update_mean_k (double &mean_k)
{
  mean_k -= (mean_k - sq_mean_k_final) * sq_mean_k_rate;
}


int main (int argc, char *argv[])
{
    if (argc != 3)
        {
            cerr << "Usage: " << argv[0] << " ifnames" << std::endl;
            return -1;
        }
    double sq_mean_k=sq_mean_k_init;
    omp_set_dynamic (0);
    std::vector<const char *> names;
    std::vector<int> cpu_ids;
    int cpu_id = 0;
    for (int i = 1; i < argc; ++i)
        {
            names.push_back (argv[i]);
            // cpu_ids.push_back ((cpu_id + 8) % 16);
            cpu_ids.push_back (-1);
            cpu_id += 1;
        }

    DaqPool pool (names, N_RAW_CH, CH_SPLIT, N_CHUNKS, cpu_ids);
    size_t old_id = 0;
    while (1)
        {
            // std::this_thread::sleep_for(std::chrono::seconds(3));
            // std::this_thread::sleep_for(5s);
            auto data = pool.fetch ();

	    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	    auto utc_now=std::gmtime(&now);
            auto id = std::get<0> (data);
	    
            std::cout << "**** fetched " << id << " skipped " << id - old_id - 1 <<" *****"<< std::endl;
            old_id = id;
            int raw_ch_beg = std::get<2> (data);
            // int raw_ch_end=std::get<3>(data);
            double freq_min = 250 / 2048. * raw_ch_beg - dnu * CH_SPLIT / 2;

            ;
            // continue;
            /*
            for(auto& i: std::get<1>(data)){
                std::cout<<i<<" ";
            }
            std::cout<<std::endl;
            */
            vector<std::complex<float>> corr (N_CH, std::complex<float>(0., 0.));
	    vector<std::complex<float>> corr_coeff (N_CH, std::complex<float>(0., 0.));
	    vector<std::complex<float>> auto1 (N_CH, std::complex<float>(0., 0.));
	    vector<std::complex<float>> auto2 (N_CH, std::complex<float>(0., 0.));
            auto p1 = std::get<1> (data)[0];
            auto p2 = std::get<1> (data)[1];


            for (size_t i = 0; i < N_CHUNKS; ++i)
                {
#pragma omp parallel for num_threads(4)
                    //#pragma omp parallel for num_threads(6)
                    for (size_t j = 0; j < N_CH; ++j)
                        {
			  auto n1=std::norm(p1[i*N_CH+j]);
			  auto n2=std::norm(p2[i*N_CH+j]);
			  

			  //if (n1<3.*sq_mean1[j] && n2<3.*sq_mean2[j])
			    {
			      auto cc=p1[i * N_CH + j] * std::conj (p2[i * N_CH + j])/std::sqrt(n1*n2);
				if(n1*n2==0)
				  {
				    cc=std::complex<float>();
				  }
				
			      corr_coeff[j]+=cc/(float)N_CHUNKS;

			      //if(std::norm(cc)<5.0*corr_coeff_sq_mean)
			      {
				corr[j] += p1[i * N_CH + j] * std::conj (p2[i * N_CH + j]) / (float)N_CHUNKS;
				auto1[j] += n1 / (float)N_CHUNKS;
				auto2[j] += n2 / (float)N_CHUNKS;
			      }
			  }
                        }
                }

	    	    
	    {
	      ofstream ofs ("corr.txt");
	      ofstream ofs1("auto1.txt");
	      ofstream ofs2("auto2.txt");
	      for (size_t i = 0; i < N_CH; ++i)
                {
		  ofs << freq_min + i * dnu << " " << corr[i].real () << " " << corr[i].imag () << std::endl;
		  ofs1 << freq_min + i * dnu << " " << auto1[i].real () <<std::endl;
		  ofs2 << freq_min + i * dnu << " " << auto2[i].real () <<std::endl;
                }
	      ofs.close ();
	      ofs1.close();
	      ofs2.close();
	    }

	    {
	      ofstream ofs("time.txt", std::ios_base::app);
	      ofs<<std::put_time(utc_now, "%Y/%m/%d %H:%M:%S")<<std::endl;
	    }

	    {
	      ofstream ofs("auto1.bin", std::ios_base::app|std::ios_base::binary);
	      ofs.write((char*)auto1.data(), sizeof(std::complex<float>)*auto1.size());
	    }

	    {
	      ofstream ofs("auto2.bin", std::ios_base::app|std::ios_base::binary);
	      ofs.write((char*)auto2.data(), sizeof(std::complex<float>)*auto2.size());
	    }

	    {
	      ofstream ofs("corr.bin", std::ios_base::app|std::ios_base::binary);
	      ofs.write((char*)corr.data(), sizeof(std::complex<float>)*corr.size());
	    }
	    {
	      ofstream ofs("coeff.bin", std::ios_base::app|std::ios_base::binary);

	      ofs.write((char*)corr_coeff.data(), sizeof(std::complex<float>)*corr_coeff.size());
	    }

	    update_mean_k(sq_mean_k);
	    std::cerr<<"sq_mean_k="<<sq_mean_k<<std::endl;
        }
}
