#!/bin/bash
ip netns add sf1
ip link set dev enp3s0f1 netns sf1
ip netns exec sf1 ip addr add 192.168.50.207/24 dev enp3s0f1
ip netns exec sf1 ip link set dev enp3s0f1 up
ip netns exec sf1 ifconfig
