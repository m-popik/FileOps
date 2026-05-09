# Documentatia Protocolului IPC

Documentul descrie arhitectura si protocolul de comunicare interprocese utilizat intre procesul manager si procesele worker din cadrul aplicatiei fileops_manager.

## 1. Arhitectura Generala
Comunicarea se bazeaza pe un model producator-consumator bidirectional. Un fisier de pe disc este mapat in spatiul de adrese al tuturor proceselor implicate cu flagul **MAP_SHARED** din functia mmap().

## 2. Layout-ul Memoriei
Datele sunt sincronizate printr-un buffer circular gestionat prin **ipc_layout_t**. Memoria partajata este structurata astfel incât sa contina antetul, cozile pentru joburi, cozile pentru rezultate si statisticile workerilor.

### 2.1 Structura Principala `ipc_layout_t`
Structura `ipc_layout_t` reprezinta blocul principal care mapeaza intregul fisier IPC:
- `magic[4]`: sirul magic de identificare a memoriei ("INV4").
- `version`: Versiunea formatului (1).
- `total_wrk`: Numarul total de workeri creati.
- `active_wrk`: Numarul de workeri activi (in viata).
- `terminate_flag`: Flag setat de ultimul worker pentru a semnala terminarea executiei.
- `pending_jobs`: Numarul total de directoare/sarcini in curs de procesare sau aflate in coada.
- `glb_mutex`: Semafor global folosit pentru modificarea in conditii de siguranta a contoarelor globale (`pending_jobs`, `terminate_flag`).
- `job_queue`: Structura care gestioneaza coada de joburi (directoare).
- `results`: Structura care gestioneaza rezultatele trimise de workeri catre manager.
- `stats`: Un array de structuri `worker_stats_t` pentru retinerea metricilor de performanta pentru fiecare worker.

## 3. Structurile de Date

### 3.1 `job_t`
Reprezinta un director care trebuie parcurs. Este un sir de caractere (cale) de lungime maxima `PATH_MAX`.
Workerii preiau elemente `job_t` din coada de joburi si le parcurg cu `opendir`/`readdir`. Când gasesc un subdirector, il adauga inapoi in coada.

### 3.2 `file_record_t`
Reprezinta metadatele unui fisier gasit. Contine:
- `path`: Calea completa catre fisier.
- `uid`, `gid`: ID-ul utilizatorului si grupului.
- `size`: Dimensiunea fisierului in bytes.
- `mtime`: Timpul ultimei modificari (Timestamp Unix).
- `mode`: Drepturile si tipul fisierului.
- `hash_sha`: Suma de control SHA256 (64 de caractere hexa + terminator null).

### 3.3 `worker_stats_t`
Structura utilizata de workeri pentru a-si salva la finalul executiei statisticile individuale:
- ID-ul workerului si PID-ul.
- Numarul de directoare procesate si de fisiere trimise.
- Numarul total de bytes parcursi.
- Timpul `wall_clock`, `user_cpu` si `sys_cpu`.

## 4. Mecanismul de Sincronizare (Cozi Circulare)

Sincronizarea foloseste trei semafoare pentru fiecare coada, implementând clasicul tipar de sincronizare Producator-Consumator.

### 4.1 Coada de Joburi (`job_queue_t`)
- **Producatori:** Managerul (pune directorul radacina) si Workerii (pun subdirectoarele descoperite).
- **Consumatori:** Workerii.
- Dimensiunea maxima a cozii: `MAX_JOB_Q_SZ` (512 elemente).
- `mutex`: Asigura accesul exclusiv la modificarea pointerilor `head` si `tail`.
- `items`: Contorizeaza numarul de elemente disponibile. Workerul se blocheaza aici daca nu exista directoare de procesat.
- `spaces`: Contorizeaza spatiile libere. Blocheaza adaugarea daca coada devine plina.

### 4.2 Canalul de Rezultate (`result_channel_t`)
- **Producatori:** Workerii (când gasesc fisiere regulate `S_ISREG`).
- **Consumator:** Managerul.
- Dimensiunea maxima a cozii: `MAX_RES_Q_SZ` (1024 elemente).
- `mutex`, `items`, `spaces`: Functioneaza identic. Permit managerului sa citeasca rezultatele pe masura ce acestea devin disponibile. Daca workerii produc date prea repede (Backpressure), acestia se vor bloca pe semaforul `spaces` pâna când managerul elibereaza loc citind datele.

## 5. Detectarea Terminarii (Evitarea Deadlock-urilor)
O parte critica a protocolului este mecanismul prin care procesele isi semnaleaza terminarea reciproc, având la baza contorul `pending_jobs`:
1. De fiecare data când un director este descoperit si adaugat in coada, `pending_jobs` creste.
2. Dupa ce un worker termina de parcurs complet un director, scade valoarea lui `pending_jobs`.
3. Când `pending_jobs` ajunge la valoarea `0`, inseamna ca **niciun worker nu mai proceseaza niciun director** si **coada de joburi este complet goala**.
4. in acest moment, worker-ul care a dus `pending_jobs` la 0 seteaza `terminate_flag = 1` si elibereaza in mod repetat semaforul `job_queue.items` pentru a-i "trezi" pe toti ceilalti workeri aflati in asteptare, ca acestia sa observe conditia de oprire si sa-si incheie executia normal.
