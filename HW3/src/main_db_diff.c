#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../include/db_format.h"
#include "../include/sync_utils.h"

void* load_to_ram(int fd, db_header_t *header, size_t record_size)
{
    if (header->record_count == 0)
        return NULL;

    size_t total_size = header->record_count *record_size;
    void *buffer = malloc(total_size);
    if(!buffer)
    {
        perror("eroare la malloc");
        exit(EXIT_FAILURE);
    }

    set_read_lock(fd, sizeof(db_header_t), total_size);

    lseek(fd, sizeof(db_header_t), SEEK_SET);
    ssize_t bytes_read = read(fd, buffer, total_size);

    release_lock(fd, sizeof(db_header_t), total_size);

    if (bytes_read != (ssize_t)total_size)
    {
        fprintf(stderr, "nu s-au putut citii toate inregistrarile\n");
        free(buffer);
        exit(EXIT_FAILURE);
    }
    return buffer;
}
void diff_files(int fd1, db_header_t *h1, int fd2, db_header_t *h2, FILE *out) {
    file_record_t *db1 = (file_record_t*) load_to_ram(fd1, h1, sizeof(file_record_t));
    file_record_t *db2 = (file_record_t*) load_to_ram(fd2, h2, sizeof(file_record_t));

    fprintf(out, "raport diff\n");
    fprintf(out, "snapshot vechi id: %lu | inregistrari: %u\n", h1->snapshot_id, h1->record_count);
    fprintf(out, "snapshot nou id: %lu | inregistrari: %u\n", h2->snapshot_id, h2->record_count);

    for (uint32_t i = 0; i < h2->record_count; i++) {
        int found = 0;
        for (uint32_t j = 0; j < h1->record_count; j++) {
            if (strcmp(db2[i].path, db1[j].path) == 0) {
                found = 1;
                if (db2[i].size != db1[j].size || db2[i].mtime != db1[j].mtime) {
                    fprintf(out, "[MODIFICAT] %s (Dimensiune: %lu -> %lu)\n", 
                            db2[i].path, db1[j].size, db2[i].size);
                }
                break;
            }
        }
        if (!found) {
            fprintf(out, "[ADAUGAT]   %s\n", db2[i].path);
        }
    }
    for (uint32_t i = 0; i < h1->record_count; i++) {
        int found = 0;
        for (uint32_t j = 0; j < h2->record_count; j++) {
            if (strcmp(db1[i].path, db2[j].path) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            fprintf(out, "[STERS]     %s\n", db1[i].path);
        }
    }

    if (db1) free(db1);
    if (db2) free(db2);
}

void diff_processes(int fd1, db_header_t *h1, int fd2, db_header_t *h2, FILE *out) {
    proc_record_t *db1 = (proc_record_t*) load_to_ram(fd1, h1, sizeof(proc_record_t));
    proc_record_t *db2 = (proc_record_t*) load_to_ram(fd2, h2, sizeof(proc_record_t));

    fprintf(out, "raport diff\n");
    fprintf(out, "snapshot vechi id: %lu | inregistrari: %u\n", h1->snapshot_id, h1->record_count);
    fprintf(out, "snapshot nou id: %lu | inregistrari: %u\n", h2->snapshot_id, h2->record_count);

    for (uint32_t i = 0; i < h2->record_count; i++) {
        int found = 0;
        for (uint32_t j = 0; j < h1->record_count; j++) {
            if (db2[i].pid == db1[j].pid) {
                found = 1;
                /* Procesul rula și înainte, dar i s-a modificat starea sau a consumat mai multă memorie */
                if (db2[i].state != db1[j].state || db2[i].rss != db1[j].rss) {
                    fprintf(out, "[MODIFICAT] PID: %u [%s] | Stare: %c->%c | RAM: %lu->%lu bytes\n", 
                            db2[i].pid, db2[i].comm, db1[j].state, db2[i].state, db1[j].rss, db2[i].rss);
                }
                break;
            }
        }
        if (!found) {
            fprintf(out, "[PORNIT]    PID: %u [%s]\n", db2[i].pid, db2[i].comm);
        }
    }

    for (uint32_t i = 0; i < h1->record_count; i++) {
        int found = 0;
        for (uint32_t j = 0; j < h2->record_count; j++) {
            if (db1[i].pid == db2[j].pid) {
                found = 1;
                break;
            }
        }
        if (!found) {
            fprintf(out, "[OPRIT]     PID: %u [%s]\n", db1[i].pid, db1[i].comm);
        }
    }

    if (db1) free(db1);
    if (db2) free(db2);
}    

int main (int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "Utilizare: %s <db1_vechi.db> <db2_nou.db> <raport_iesire.txt>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int fd1 = open(argv[1], O_RDONLY);
    int fd2 = open(argv[2], O_RDONLY);

    if (fd1 == -1 || fd2 == -1)
    {
        perror("fisierele nu au putut fi deschise");
        return EXIT_FAILURE;
    }

    db_header_t h1, h2;
    read(fd1, &h1, sizeof(db_header_t));
    read(fd2, &h2, sizeof(db_header_t));

    if (h1.magic != DB_MAGIC_NUMBER || h2.magic != DB_MAGIC_NUMBER) {
        fprintf(stderr, "eroare: unul din fisiere (sau ambele) nu sunt baze de date valide\n");
        close(fd1); close(fd2);
        return EXIT_FAILURE;
    }

    struct stat st1, st2;
    fstat(fd1, &st1);
    fstat(fd2, &st2);

    size_t record_size = 0;
    if (h1.record_count > 0) {
        record_size = (st1.st_size - sizeof(db_header_t)) / h1.record_count;
    } else if (h2.record_count > 0) {
        record_size = (st2.st_size - sizeof(db_header_t)) / h2.record_count;
    }

    FILE *out = fopen(argv[3], "w");
    if (!out) {
        perror("eorare creare raport");
        return EXIT_FAILURE;
    }

    if (record_size == sizeof(file_record_t)) {
        diff_files(fd1, &h1, fd2, &h2, out);
    } else if (record_size == sizeof(proc_record_t)) {
        diff_processes(fd1, &h1, fd2, &h2, out);
    } else {
        fprintf(out, "bazele de date sunt goale sau nu pot fi definite\n");
    }

    fclose(out);
    close(fd1);
    close(fd2);

    return EXIT_SUCCESS;
}