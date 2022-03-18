#!/bin/bash
experiment_name="data_gen"
program_name="python3 pcap_gen.py"
benchmark_folder="./gen10000"
# Benchmark set examples: "inst_4x4_5_*" or "*_5_0"
benchmark_set_name="cur_arb"
log_file="${experiment_name}_${benchmark_set_name}_Log.txt"

date -Iseconds >> ${log_file}
echo "Data generation starts" >> ${log_file}
echo "" >> ${log_file}

# Check if the folder exists: https://www.cyberciti.biz/faq/check-if-a-directory-exists-in-linux-or-unix-shell/
if ! [ -d "$benchmark_folder" ]; then
    mkdir ${benchmark_folder}
    echo "New folder ${benchmarkfolder} created." >> ${log_file}
fi

# Loop numbers: https://unix.stackexchange.com/questions/247497/how-to-run-a-command-for-each-number-in-a-range
# Loop files: https://stackoverflow.com/questions/20796200/how-to-loop-over-files-in-directory-and-change-path-and-add-suffix-to-filename
for i in {0..9999}
do
    # Original command:
    # ./timeout ./QuEST_qasm_sim --sim_qasm=../../../SliQSim/benchmarks/simulation/supremacy/GRCS_qasm_depth_6/inst_4x4_5_0.qasm -p"
    command="${program_name} g --req_arb --output_csv data${i}.csv >> data${i}.txt"

    # Echo and execute the command
    # No redirection from variable: https://stackoverflow.com/questions/40414662/how-do-i-redirect-output-when-the-command-to-execute-is-stored-in-a-variable-in
    echo "${command}" >> ${log_file}
    ${program_name} g --req_arb --output_csv ${benchmark_folder}data${i}.csv >> ${benchmark_folder}/data${i}.txt
    echo "" >> ${log_file}
done

date -Iseconds >> ${log_file}
echo "Data generation finishes" >> ${log_file}