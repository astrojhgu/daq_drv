#!/bin/sh
#export GOMP_CPU_AFFINITY=0-2
#export OMP_PROC_BIND=true 
taskset -c 0-5 nice -n -20 ./multi ens5f0 ens5f1
#sudo taskset -c 8-13 nice -n -20 ./multi ens5f0 ens5f1
