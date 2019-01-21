#!/bin/sh
#export GOMP_CPU_AFFINITY=0-2
#export OMP_PROC_BIND=true
sudo rm -f /dev/shm/*
rm -f devs.txt
ip token|awk '{print $4}'|while read dev;
do
    dri=`ethtool -i $dev |grep driver|awk '{print $2}'`
    if [ $dri == 'ixgbe' ]
    then
	echo $dev >>devs.txt
#	echo $devs
    fi
done

devs=`cat devs.txt`
#sudo taskset -a -c 0-5 nice -n -20 ./multi $devs
#sudo taskset -a -c 9-15 nice -n -20 ./multi $devs
#sudo numactl -N 1 --localalloc ./multi $devs
sudo numactl -N 1 --interleave=all ./multi $devs

#sudo taskset -c 8-13 nice -n -20 ./multi ens5f0 ens5f1
#sudo numactl --localalloc taskset -a -c 8-15 nice -n -20 ./multi $devs
