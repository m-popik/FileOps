# Specificația Formatului Binar DB (SNAP)

Prezentul document descrie structura internă a fișierelor bazei de date binară utilizate în proiectul **Tema 3 - Sisteme de Operare**. Acest format este conceput pentru a permite accesul concurent eficient și pentru a asigura integritatea datelor prin utilizarea lacătelor la nivel de fișier (*file locking*).

---

## 1. Arhitectura Generală
Fișierul este compus din două secțiuni principale:
1. **Header-ul Bazei de Date**: O structură de dimensiune fixă plasată la începutul fișierului (offset 0).
2. **Zona de Date**: O succesiune de înregistrări de dimensiune fixă (Records), dispuse imediat după header.

Toate structurile sunt aliniate folosind atributul `__attribute__((packed))` pentru a evita spațiile goale (*padding*) adăugate de compilator, asigurând un format dens și portabil.

---

## 2. Header-ul Comun (`db_header_t`)

Fiecare fișier (fie el de indexare fișiere sau snapshot procese) începe cu acest header de **25 octeți**.

| Câmp | Tip | Dimensiune | Descriere |
| :--- | :--- | :--- | :--- |
| `magic` | `uint32_t` | 4 bytes | Identificator format: `0x534E4150` ("SNAP"). |
| `format_version` | `uint32_t` | 4 bytes | Versiunea schemei (implicit `1`). |
| `snapshot_id` | `uint64_t` | 8 bytes | Timestamp Unix reprezentând momentul creării. |
| `snapshot_state` | `uint8_t` | 1 byte | Starea: `0` (In Progress), `1` (Sealed). |
| `active_writers` | `uint32_t` | 4 bytes | Numărul de procese care scriu în prezent. |
| `record_count` | `uint32_t` | 4 bytes | Numărul total de înregistrări stocate. |

---

## 3. Înregistrările de Date

Tipul de înregistrare depinde de contextul bazei de date. Un singur fișier conține doar un singur tip de înregistrare.

### 3.1. Înregistrare Fișier (`file_record_t`)
Utilizată în `index.db`. Dimensiune fixă per înregistrare: **~4190 bytes** (în funcție de `PATH_MAX`).

* **path**: `char[4096]` - Calea absolută către fișier.
* **type**: `uint8_t` - Tipul (Regular, Directory, Link, Other).
* **size**: `uint64_t` - Dimensiunea în octeți.
* **mtime**: `uint64_t` - Data ultimei modificări (Unix timestamp).
* **hash**: `char[65]` - Hash SHA256 (format string hex).
* **st_dev**: `uint64_t` - Identificatorul dispozitivului.
* **st_ino**: `uint64_t` - Numărul Inode.

### 3.2. Înregistrare Proces (`proc_record_t`)
Utilizată în `proc.db`. Dimensiune fixă per înregistrare: **297 bytes**.

* **pid**: `uint32_t` - Process ID.
* **ppid**: `uint32_t` - Parent Process ID.
* **state**: `char` - Starea procesului (R, S, D, Z, T).
* **comm**: `char[16]` - Numele scurt al executabilului.
* **cmdline**: `char[256]` - Linia de comandă completă.
* **rss**: `uint64_t` - Memoria rezidentă (RSS) în bytes.
* **cpu_time**: `uint64_t` - Suma `utime` + `stime`.

---

## 4. Mecanismul de Sincronizare

Pentru a gestiona accesul concurent, se aplică următoarele reguli folosind `fcntl()`:

1.  **Actualizarea Header-ului**: Se aplică un lacăt de scriere (`F_WRLCK`) pe primii 25 de octeți. Se incrementează `record_count` și se eliberează lacătul.
2.  **Scrierea Datelor**: Se calculează offset-ul: `sizeof(header) + (old_count * sizeof(record))`. Se aplică un lacăt de scriere pe regiunea specifică noii înregistrări.
3.  **Finalizarea (Sealing)**: Când un proces termină, scade `active_writers`. Dacă acesta devine `0`, `snapshot_state` este setat pe `1` (SEALED).

---

## 5. Diferențierea (Diff)

Utilitarul `db_diff` identifică tipul bazei de date calculând dimensiunea unei înregistrări:
`dim_record = (dim_total_fisier - sizeof(header)) / record_count`

Dacă `dim_record` se potrivește cu una dintre structurile definite, se pornește algoritmul de comparare specific.
