#!/bin/bash
ip netns add sf0
ip link set dev enp3s0f0 netns sf0
ip netns exec sf0 ip addr add 192.168.20.100/24 dev enp3s0f0
ip netns exec sf0 ip link set dev enp3s0f0 up
ip netns exec sf0 ifconfig
