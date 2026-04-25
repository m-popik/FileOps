Cerinta 2.A1)

ls - Listeaza continutul directorului curent. -l listeaza in format long. -a nu ignora intrarile ce incep cu “.”.

<> ls -la

Output: reports/fs/A1_ls_long.txt

cerinta 2.A2)

find – Cauta fisierele din ierarhia directorului. “.” precizeaza directoriul curent. -name cauta prin pattern matching fisierele ce se termina in “.sh”

<> find . -name “*.sh” 

Output: reports/fs/A2_find_sh.txt

Cerinta 2.A3)

du – estimeaza cat spatiu ocupa un fisier. -h face outputul citibil. -max-depth=1 limiteaza comanda la directoarele de nivel 1.

<> du -h –max-depth=1 

Output: reports/fs/A3_du_level1.txt

Cerinta 2.B1

ps – arata un snapshot al proceselor curente --sort sorteaza descrescator dupa cata memorie consuma, --cols limiteaza numarul de coloane la 20 . Head arata primele 10 intrari+ headerul(user,pid...).

Nu am rezolvat cu top pentru ca top arata date “live” si e mai greu de transmis in fisier.

<> ps -eo user,pid,%mem,cmd –sort=-%mem –-cols 20 | head -n 11 

Output: reports/process/B1_top_mem.txt

Cerinta 2.B2

pstree - afiseaza arborele de procese pentru sistem. -p face PID urile vizibile.

<> pstree -p 

Output: reports/process/B2_pstree.txt

Cerinta 2.B3

pornim un proces in background cu sleep si il identificam dupa nume si pid cu pgrep. -fl printeaza doar fisierele cu match.

<> sleep 60 & pgrep -fl sleep 

poate fi oprit cu “pkill sleep” sau “kill [PID]”

Output: reports/process/B3_pgrep_sleep.txt

Cerinta 2.C1

extrage modelul CPU din /proc/cpuinfo cautand primul match din fisier

<> grep -m 1 “model name” /proc/cpuinfo 

Output: reports/proc/C1_cpu_model.txt

Cerinta 2.C2

extrage “memtotal” si “memavailable” din /proc/meminfo. -E interpreteaza patternurile ca fiind regex.

<> grep -E “MemTotal|MemAvailable” /proc/meminfo 

Output: reports/proc/C2_mem_total_avail.txt

Cerinta 2.C3

afiseaza uptime in secunde din /proc/uptime. Delimiteaza prin spatii

uptime -r

<> cut -d ‘ ‘ -f 1 /proc/uptime 

Output: reports/proc/C3_uptime.txt

Cerinta 2.D1

construieste topul celor mai mari 5 fisiere is afiseaza doar dimensiunea si calea

<> du -a .. | sort -nr | head -n 6 

Output: reports/pipeline/D1_top5_large_files.txt

Cerinta 2.D2

Construieste topul celor mai mari 5 procese dupa memorie, extrage PID-ul si numele si elimina duplicatele limitand outputul la 5 linii. Am folosit grep pentru a sterge antetul si a pastra 5 linii din output.

<> ps -eo pid,comm --sort=-%mem | grep -v "PID" | head -n 5 | uniq

Output: reports/pipeline/D2_top5_proc_mem_pid_name.txt

Cerinta 2.D3

din acest fisier extragem liniile ce contin comenzi, le sortam si le numaram.

<> grep “<> ” doc/T1_comenzi.md | sort | wc -l

Output: reports/pipeline/D3_count_commands.txt

Pornit : 2026-03-08 18:21:10
Oprit : 2026-03-08 18:21:10
Pornit : 2026-03-08 18:23:36
Oprit : 2026-03-08 18:23:36
Pornit : 2026-03-08 18:25:26
Oprit : 2026-03-08 18:25:26
Pornit : 2026-03-11 18:23:21
Oprit : 2026-03-11 18:23:21
Pornit : 2026-03-11 19:24:56
Oprit : 2026-03-11 19:24:56
