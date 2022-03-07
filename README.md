# Xilinx ACC 2021 Submission
EXTREME TRADING SOLUTION: Quantum-accelerated trading strategies on Accelerated Algorithmic Trading (AAT) Framework

## 1-1 Project Abstraction
Quantum Bifurcation Machines can be used for quantum adiabatic optimization and universal quantum computation. TOSHIBA has realized Quantum Bifurcation Machines to Simulated Bifurcation Machine（SBM）Technologies accelerated with NVIDIA GPU.
We design and implement Quantum-accelerated Trading Strategies for currency arbitrage with replaceable algorithms modular（Simulated Bifurcation, SB and Simulated Quantum Annealing, SQA）on Xilinx Accelerated Algorithmic Trading (AAT) framework, which is a Fully featured open source reference design for trading applications.

## 1-2 Data Generation

## 1-3 Synthesis & Compiling
Development Environment:
* Operation System: Ubuntu 20.04.2 LTS
* Xilinx Vitis Software Platform 2021.1
* Xilinx Accelerated Algorithmic Trading reference design package Q2 (UG1067 v1.1, July 2 2021)

Settings in ~/.bashrc:

    source /opt/Xilinx/Vitis/2021.1/settings64.sh
    source /opt/xilinx/xrt/setup.sh
    export PLATFORM_REPO_PATHS='/opt/xilinx/platforms'
    export LM_LICENSE_FILE="~/Xilinx.lic"
    export XILINX_PLATFORM='xilinx_u50_gen3x16_xdma_201920_3'
    export DEVICE=${PLATFORM_REPO_PATHS}/${XILINX_PLATFORM}/${XILINX_PLATFORM}.xpfm
    export DM_MODE=DMA
    
 Synthesis & Compiling instructions:

    $ cd ../Accelerated_Algorithmic_Trading/build
    $ make clean
    $ ./buildall.sh
    
## 1-4 Test Flow
Preparation:
* A x86 host installed with a Xilinx Avelon U50 accelrator
* A x86 host installed with a Broadcom BCM957711A 10Gb x 2 SFP port card and PCAP test files
* A QSFPx1-to-SFPx4 cable
* AAT demo_setup.cfg and Network_setting can be found in Configuration directory

A reference configuration used by the Xilinx verification team.

<img src="https://user-images.githubusercontent.com/11850122/155674938-61f34770-496f-43bc-8310-6f91ae20ce40.png" width=60%>

Running Quantum-accelerated AAT shell on U50 host terminal.

    sudo reboot (if needed to clean U50 setting)
    cd ../Accelerated_Algorithmic_Trading/build
    vim support/demo_setup.cfg (if default u50 network setting needed to be changed)
    ./aat_shell_exe
    download ./sample/aat.u50_xdma.xclbin
    run support/demo_setup.cfg
    datamover threadstart
    udpip0 getstatus
    
Running Linux Netcat command to get Quantum-accelerated AAT output on Broadcom host terminal#1.

    cd ../Network_setting/
    sudo ./settingNetwork_sf0.sh
    sudo ./execFrom_sf0.sh ping -w 5 192.168.20.200 (optional test)
    sudo ./execFrom_sf0.sh nc -n -l 192.168.20.100 12345 -v
    
If Linux Netcat has not shown Quantum-accelerated AAT connection IP & Port message, run reconnection on U50 host terminal.

    orderentry reconnect
    orderentry getstatus

From U50 host terminal, connection established should be shown "true" and connection status should be shown "SUCCESS”.

<img src="https://user-images.githubusercontent.com/11850122/155680914-ad137fe7-37af-4048-a270-ee72ed263c0e.png" width=45%>

Running Linux TCPreplay command to send Quantum-accelerated AAT input from Broadcom host terminal#2.

    cd ../Network_setting/
    sudo ./settingNetwork_sf1.sh
    sudo ./execFrom_sf1.sh ping -w 5 192.168.50.101 (optional test)
    sudo ./execFrom_sf1.sh tcpreplay --intf1=enp3s0f1 --pps=2 --stats=1 ../Accelerated_Algorithmic_Trading/build/sample/cme_input_arb.pcap

## 2-1 Currency Arbitrage QUBO Formulation

## 2-2 SQA Design & Implementation

## 2-3 SBM Design & Implementation
SBM-accelerated trading strategy for the Xilinx AAT platform.
https://github.com/bol-edu/xilinx-acc-2021_submission/tree/main/sbm

## 3 Features and Benefits
Quantum-accelerated Trading Strategies:
* Leverage HLS to develop trading strategies with readability and maintainability
* Customizable Quantum-accelerated trading strategies
* Parallel computing on FPGA hardware
* Built on trading industry-standard of CME and FIX

Accelerated Algorithmic Trading (AAT) Framework:
* Full implementation in HLS
* Designed for software engineers
* Source code provided
* Integrated with Vitis™
* Runs out of the box on Xilinx Alveo™
* Supported on Alveo™ U250, U50
* Supports XDMA and host memory bridge-based

Quantum-accelerated trading strategies can be executed on CPUs/GPUs but incur additional latency particularly from traversing the PCIe bus. FPGA based trading strategies can significantly lower latency, but typically need large teams of hardware experts and long design cycles.
Xilinx AAT reference design can provide all the infrastructure required to create a Quantum-accelerated trading application on the FPGA using Xilinx Vitis™ unified platform, and standard Xilinx shells. The design is written in HLS, and all the source code is provided. The design is modular, allowing easily replacing trading strategy IP blocks in the reference design with needed strategy.

<img src="https://user-images.githubusercontent.com/11850122/155683919-d6f0f33b-53d5-418b-95b4-2e1da4f79e07.png" width=120%>

Enhancement in AAT shell:
* Status/Debugging registers for SQA/SBM modules
* Add timestamp and time pause in both shell mode and run script mode

  <img src="https://user-images.githubusercontent.com/11850122/155716224-b657dfe2-7f4a-4e56-8aab-4fef7bff3ce4.png" width=55%>

