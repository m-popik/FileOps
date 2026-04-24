#!/bin/bash

echo "Pornit : $(date '+%Y-%m-%d %H:%M:%S')" >> ../doc/T1_comenzi.md
mkdir ../reports/fs
mkdir ../reports/process
mkdir ../reports/proc
mkdir ../reports/pipeline

ls -la > ../reports/fs/A1_ls_long.txt
find . -name "*.sh" > ../reports/fs/A2_find_sh.txt
du -h --max-depth=1 > ../reports/fs/A3_du_level1.txt

ps -eo user,pid,%mem --sort=-%mem --cols 20 | head -n 11 > ../reports/process/B1_top_mem.txt
pstree -p > ../reports/process/B2_pstree.txt
sleep 60 & pgrep -fl sleep > ../reports/process/B3_pgrep_sleep.txt

grep -m 1 "model name" /proc/cpuinfo > ../reports/proc/C1_cpu_model.txt
grep -E "MemTotal|MemAvailable" /proc/meminfo > ../reports/proc/C2_mem_total_avail.txt
cut -d ' ' -f 1 /proc/uptime > ../reports/proc/C3_uptime.txt

du -a .. | sort -nr | head -n 6 > ../reports/pipeline/D1_top5_large_files.txt
ps -eo pid,comm --sort=-%mem | grep -v "PID" | head -n 5 | uniq > ../reports/pipeline/D2_top5_proc_mem_pid_name.txt
grep "<> " ../doc/T1_comenzi.md | sort | wc -l > ../reports/pipeline/D3_count_commands.txt

echo "Oprit : $(date '+%Y-%m-%d %H:%M:%S')" >> ../doc/T1_comenzi.md

{   
    echo "reports/fs"
    echo "reports/process"
    echo "reports/proc"
    echo "reports/pipeline"
} > ../reports/T1_summary.txt