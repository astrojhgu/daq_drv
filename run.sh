#!/bin/bash
#export GOMP_CPU_AFFINITY="10 11 12 13"
#export GOMP_CPU_AFFINITY="0 1 2 3"
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
dev1=`head -1 devs.txt`
#exit
node=`numactl --prefer netdev:${dev1} --show|grep 'preferred node'|awk '{print $3}'`
echo $node

#sudo taskset -a -c 0-7 nice -n -20 ./multi $devs
#sudo taskset -a -c 8-12 nice -n -20 ./multi $devs
#sudo numactl -N $node --localalloc ./multi_gpu $devs
sudo numactl -N $node ./single eth2
#sudo numactl -N netdev:$dev1 --localalloc ./multi $devs
#sudo numactl -N $node --interleave=all ./multi $devs

#sudo taskset -c 8-13 nice -n -20 ./multi ens5f0 ens5f1
#sudo numactl --localalloc taskset -a -c 8-15 nice -n -20 ./multi $devs
#sudo  numactl --interleave=all ./multi $devs
#numactl --localalloc ./multi $devs
#numactl --preferred=netdev:$dev1 ./multi $devs
