#!/bin/bash

# $1 Directory
# $2 Server Ethernet ip
# $3 Client Ethernet ip
# $4 Server Infiniband ip


pkgSize=1024
numPkg=1000
name=coNetPerf
limitPkgSize=`expr $(expr 32 \* $(expr 1024 \* 1024)) + 1`

################### Ethernet ###################

# TCPIP
while [ $pkgSize -lt $limitPkgSize ]
do
commandServer="cd $1 && ./bin/coNetperf -s $2:4343:TCPIP -a"
commandClient="cd $1 && ./bin/coNetperf -c $2:4343:TCPIP -a -n $numPkg -p $pkgSize >> $HOME/ethernetTCPIP$name.txt"
echo $commandServer
echo $commandClient
ssh $2 $commandServer &> /dev/null &
sleep 3
ssh $3 $commandClient
pkgSize=`expr $pkgSize \* 2`
done

################### Infiniband ###################

# TCPIP
pkgSize=1024
while [ $pkgSize -lt $limitPkgSize ]
do
commandServer="cd $1 && ./bin/coNetperf -s $4:4343:TCPIP -a"
commandClient="cd $1 && ./bin/coNetperf -c $4:4343:TCPIP -a -n $numPkg -p $pkgSize >> $HOME/infinibandTCPIP$name.txt"
echo $commandServer
echo $commandClient
ssh $2 $commandServer &> /dev/null &
sleep 3
ssh $3 $commandClient
pkgSize=`expr $pkgSize \* 2`
done

#RDMA
pkgSize=1024
while [ $pkgSize -lt $limitPkgSize ]
do
commandServer="cd $1 && ./bin/coNetperf -s $4:4343:RDMA -a"
commandClient="cd $1 && ./bin/coNetperf -c $4:4343:RDMA -a -n $numPkg -p $pkgSize >> $HOME/infinibandRDMA$name.txt"
echo $commandServer
echo $commandClient
ssh $2 $commandServer &> /dev/null &
sleep 3
ssh $3 $commandClient
pkgSize=`expr $pkgSize \* 2`
done

################### MPI ###################

while [ $pkgSize -lt $limitPkgSize ]
do
srun -n 2 $1./bin/coNetperf -a -n $numPkg -p $pkgSize >> $HOME/MPI$name.txt
pkgSize=`expr $pkgSize \* 2`
done
