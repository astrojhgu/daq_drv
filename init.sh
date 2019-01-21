#!/bin/sh

#sudo modprobe netmap
sudo ifconfig ens5f0 promisc mtu 9000
sudo ifconfig ens5f1 promisc mtu 9000

sudo ifconfig ens5f0 192.168.2.2
sudo ifconfig ens5f1 192.168.3.2
exit

#sudo ifconfig ens3f0 mtu 9000
#sudo ethtool --pause ens5f0 autoneg on tx on rx on
sudo ethtool --pause ens5f0 autoneg off tx off rx off
sudo ethtool --pause ens5f1 autoneg off tx off rx off
ethtool -a ens5f0
ethtool -a ens5f1

sudo sysctl -w net.core.wmem_max=2147483647
sudo sysctl -w net.core.rmem_max=2147483647
sudo sysctl -w net.core.rmem_default=1073741824
#sudo sysctl -w net.ipv4.udp_rmem_min=8192000 
sudo sysctl net.core.wmem_max
sudo sysctl net.core.rmem_max
sudo sysctl net.core.rmem_default
sudo sysctl net.ipv4.udp_rmem_min
