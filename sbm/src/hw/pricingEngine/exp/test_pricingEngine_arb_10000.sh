#!/bin/bash
experiment_name="test_pricingEngine"
program_name="../tb_pricingEngine"
benchmark_folder="../../../../../temp/xilinx-acc-2021_submission/test_toolkit/gen10000"
# Benchmark set examples: "inst_4x4_5_*" or "*_5_0"
benchmark_set_name="cur_arb_5_9"
log_file="${experiment_name}_${benchmark_set_name}_Log.txt"

date -Iseconds >> ${log_file}
echo "Experiment starts" >> ${log_file}
echo "" >> ${log_file}

# Loop numbers: https://unix.stackexchange.com/questions/247497/how-to-run-a-command-for-each-number-in-a-range
# Loop files: https://stackoverflow.com/questions/20796200/how-to-loop-over-files-in-directory-and-change-path-and-add-suffix-to-filename
for i in {0..9999}
do
    # Original command:
    # ./timeout ./QuEST_qasm_sim --sim_qasm=../../../SliQSim/benchmarks/simulation/supremacy/GRCS_qasm_depth_6/inst_4x4_5_0.qasm -p"
    file="${benchmark_folder}/data${i}.txt"
    command="${program_name} ${file} &>> ${log_file}"

    # Echo and execute the command
    # No redirection from variable: https://stackoverflow.com/questions/40414662/how-do-i-redirect-output-when-the-command-to-execute-is-stored-in-a-variable-in
    echo "${command}" >> ${log_file}
    ${program_name} ${file} &>> ${log_file}
    echo "" >> ${log_file}
done

date -Iseconds >> ${log_file}
echo "Experiment finishes" >> ${log_file}