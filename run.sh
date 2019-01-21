make
sudo taskset -c 0-7 nice -n -20 ./multi ens5f0 ens5f1
