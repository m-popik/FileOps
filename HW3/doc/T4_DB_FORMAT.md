# Specificatia formatului binar al bazei de date

Acest document descrie dispunerea datelor in format binar pentru a face intelegerea proiectului mai usoara. Aici explic cum managerul scrie si valideaza fisierul final.

---

## Formatul bazei de date

Baza de date binara finala stocheaza rezultatele inventarierii si statisticile executiei. Construita si publicata doar de **fileops_manager** prin scriere atomica. Folosim aceasta tehnica deoarece asigura consistenta resurselor generate.

### Structura generala

Fisierul binar sete compus din 3 sectiuni scrise fara spatii:
1. **Header**

2. **File records** (inregistrarile fisierelor regulate gasite)

3. **Worker Statistics** (statisticile de executie pentru procesele worker)

### Descrierea structurilor

#### Header
Antetul are o dimensiune fixa si contine informatii globale despre rezultatele inventarierii.

|Camp|Tip de date| Descriere
|----|----|----|
| magic | char[4] | Identificator. Contine valoarea "INV4".|
|format_version|uint32_t|Versiunea formatului binar (initial 1).|
|complete| uint32_t | Flag de stare (1-terminat, 0-execuite intrerupta)|
|file_record_count|uint32_t|Nr. total de **file_record** existente in sectiunea uramtoare (un pseudo-separator ce functioneaza ca un indicator predictiv)|
|worker_count|uint32_t|Nr. total de procese worker instantiate de manager|

#### File Records
Un vector contiguu de structuri ce contin metadatele. Marimea variaza in fc. de numarul de fisiere regulate descoperite de workeri.

|Camp|Tip de date|Descriere|
|----|----|----|
|path|char[PATH_MAX]|Calea absoluta|
|size|uint64_t|Dimensiunea curenta in bytes|
|mtime|uint64_t/time_t|Timpul ultimei modificari|
|mode|mode_t|Permisiunile fisierului|
|uid|uid_t|ID User proprietar|
|gid|gid_t|ID Grup proprietar|
|hash_sha256|char[65]|SHA256 calculat pentru contin utul fisierului. Sir de caractere terminat cu null.|

#### Worker Stats
Dupa terminarea file-recordurilor baza de date stocheaza un vector de structuri cu informatii despre procesele worker. Dimensiune dictata de **worker_count** din header
|Camp|Tip de date|Descriere|
|----|----|----|
|worker_id|int32_t|Identificator atribuit procesului(0->N-1)|
|pid|pid_t|Id proces(alocat de sistemul de operare)|
|exit_status|int32_t|statusul de terminare preluat prin waitpid|
|jobs_processed|uint32_t|Nr total de directoare parcurse cu succes de **worker_id**|
|files_emitted|uint32_t|Nr. de fisiere regulate descoperite si transmise prin canalul de rezlutate.|
|bytes_emitted|uint64_t|Dimensiunea cumulata a fisierelor identificate de worker.|
|wall_time_ms|uint64_t|Timpul fizic masurat de la inceputul pana la sfarsitul executiei.|
|user_cpu_us|uint64_t|Timpul de executie in spatiul utilizator|
|sys_cpu_us|uint64_t|Timpul de executie dedicat apelurilor de sistem|

### Criterii de validare

Optiunea --verify din CLI forteaza managerul sa analizeeze baza de date. Procesul de validare se bazeaza pe consistenta acestui format si trebuie sa garanteze urm. criterii:
1. Prezenta magic-ului
2. Validarea versiunii
3. Dimensiunea
