# T5 - Control Plane si Shutdown Gratios

Acest document descrie arhitectura *control plane*-ului introdusa in Tema 5, mecanismul de raportare a statusului si pasii executati pentru o oprire sigura (graceful shutdown) a inventarierii.

## 1. Canalul de Control (Pipe anonim)
Fluxul principal de date (job-uri, file records, statistici) se desfasoara in continuare prin memoria partajata (MMAP).
Pentru raportarea evenimentelor de catre workeri, s-a adaugat un **pipe anonim**, care functioneaza ca un canal de comunicare unidirectional (worker to manager). 

### 1.1 Atomicitate si Conexiune
- Managerul creeaza pipe-ul folosind `pipe()` inainte de a face `fork()` pentru workeri.
- Capatul de scriere este mostenit de toti workerii si transmis prin parametrul `--control-fd`.
- Managerul inchide capatul de scriere si foloseste capatul de citire in mod **O_NONBLOCK**. 
- Deoarece fiecare mesaj este mai mic de `PIPE_BUF` (4096 octeti), toate scrierile (apeluri `write()`) sunt complet atomice.

### 1.2 Formatul T5MSG
Mesajele au un format text predefinit, incep cu token-ul `T5MSG`, continua cu perechi `cheie=valoare` si se termina cu `\n`:
- `T5MSG type=JOB_DONE worker_id=<id> done` (trimis de worker la finalizarea parcurgerii unui director).
- `T5MSG type=WORKER_EXITING worker_id=<id> reason=shutdown` (trimis de worker când primeste semnal de oprire).

## 2. Raportarea Statusului (SIGUSR1)
La primirea semnalului `SIGUSR1`, managerul seteaza un flag `sig_atomic_t`. Bucla sa principala detecteaza acest flag si afiseaza la stdout o linie de progres de tipul:
`STATUS queued_jobs=<n> active_jobs=<n> files=<n> bytes=<n> workers_alive=<n> complete=0`
- **queued_jobs**: Numarul de job-uri ramase in coada.
- **files**: Numarul de fisiere regulate identificate pâna in acel moment.
- **bytes**: Dimensiunea totala a fisierelor identificate.
- **workers_alive**: Numarul de procese worker care inca ruleaza.

## 3. Graceful Shutdown (SIGINT/SIGTERM)
Managerul trateaza `SIGINT` si `SIGTERM` pentru a opri inventarierea fara a lasa procese zombie sau a corupe structurile de date.
1. Handler-ul managerului intercepteaza semnalul si seteaza un flag global.
2. Managerul opreste alocarea de noi activitati si marcheaza variabila `terminate_flag` din MMAP ca fiind `1`.
3. Managerul elibereaza toate semafoarele (`sem_post` de `N` ori pe `job_queue.items`) pentru a trezi workerii blocati, apoi le trimite semnalul `SIGTERM`.
4. Dupa un timeout configurabil (`--graceful-timeout`), managerul verifica daca mai exista workeri activi si le trimite `SIGKILL`.
5. Workerii care prind `SIGTERM` ies din functie devreme, scriind in control pipe mesajul `WORKER_EXITING`.

## 4. Semantica complete=0
Daca inventarierea a fost intrerupta prematur, fisierul `inventory.db` tot va fi scris atomic pe disc, dar valoarea flag-ului `complete` din antet va fi `0`.
Aceasta indica o baza de date structural perfect valida, dar cu o inventariere neterminata.
Managerul foloseste `rename` dintr-un fisier temporar pentru a scrie baza de date si ofera validare folosind argumentul `--verify`. Flag-ul de complete poate fi vazut utilizând parametrul `--dump`.
