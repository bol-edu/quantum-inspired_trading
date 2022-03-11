## Build AAT Shell Software with Enhancement
Replace origional AAT Q2 xlnx_cmd_manager.cpp with submitted xlnx_cmd_manager.cpp

    $ cd ../Accelerated_Algorithmic_Trading/sw/framework/shell
    $ mv ./xlnx_cmd_manager.cpp ./xlnx_cmd_manager.cpp.bak
    $ cp -f ../xilinx-acc-2021_submission/aat_shell_enhancement/src/sw/framework/shell/xlnx_cmd_manager.cpp ./
  
Build att_shell_exe with new timestamp and time control functions

    $ cd ../Accelerated_Algorithmic_Trading/sw/applications/aat/aat_shell_exe
    $ make all

<img src="https://user-images.githubusercontent.com/11850122/155716224-b657dfe2-7f4a-4e56-8aab-4fef7bff3ce4.png" width=50%>
