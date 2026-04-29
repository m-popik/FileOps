#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "../include/db_format.h"
#include "../include/sync_utils.h"

//verificam daca fisierul este pid
int is_num(const char *str)
{
    for (int i=0; str[i] != '\0'; i++)
    {
        if(!isdigit(str[i]))
        return 0;
    }
    return 1;
}

//aceeeasi fct ca la main_fileops_indexer.c

void write_to_db(int db_fd, proc_record_t *record)
{
    db_header_t header;

    set_write_lock(db_fd, 0, sizeof(db_header_t));
    lseek(db_fd, 0, SEEK_SET);
    read(db_fd, &header, sizeof(db_header_t));

    /*calcul inregistrare noua*/
    off_t new_offset = sizeof(db_header_t) + (header.record_count * sizeof(proc_record_t));
    
    /*actualizare header*/
    header.record_count++;
    lseek(db_fd, 0, SEEK_SET);
    write(db_fd, &header, sizeof(db_header_t));

    /*deblocare header*/
    release_lock(db_fd, 0, sizeof(db_header_t));

    /*blocam zona inregistrarii si scriem*/
    set_write_lock(db_fd, new_offset, sizeof(proc_record_t));

    lseek(db_fd, new_offset, SEEK_SET);
    write(db_fd, record, sizeof(proc_record_t));
    release_lock(db_fd, new_offset, sizeof(proc_record_t));
}

void proc_parsing(const char *pid_str, int db_fd)
{
    proc_record_t record;
    memset(&record, 0, sizeof(proc_record_t));
    record.pid = atoi(pid_str);

    char path[PATH_MAX];
    char line[1024];

    snprintf(path, sizeof(path), "/proc/%s/stat", pid_str);
    FILE *fstat = fopen(path, "r");
    if (fstat)
    {
        if(fgets(line, sizeof(line), fstat)){
        char *lparanteza = strchr(line, '(');
        char *rparanteza = strchr(line, ')');

        if(rparanteza && rparanteza && rparanteza > lparanteza)
        {
            int comm_len = rparanteza - lparanteza -1;
            if (comm_len >= MAX_COMM_LEN) comm_len = MAX_COMM_LEN - 1;
            strncpy(record.comm, lparanteza+1, comm_len);

            //continuam dupa a doua paranteza

            char *rest = rparanteza + 2;
            unsigned long utime, stime;
            long rss_pages;

            /*stat are 52 de campuri, folosesc sscanf cu assignment supression
            citeste de pe 3 si 4, sare 9, citeste 14, 15, sare 8, citeste 24*/
            sscanf(rest, "%c %u %*s %*s %*s %*s %*s %*s %*s %*s %*s %lu %lu %*s %*s %*s %*s %*s %*s %*s %*s %ld", 
                   &record.state, &record.ppid, &utime, &stime, &rss_pages);

                record.cpu_time = utime + stime;

                long page_size = sysconf(_SC_PAGESIZE);
                record.rss = (uint64_t)rss_pages * page_size;
        }
    }
    fclose(fstat);
    }

    snprintf(path, sizeof(path), "/proc/%s/cmdline", pid_str);
    int fd_cmd = open(path, O_RDONLY);
    if (fd_cmd != -1)
    {
        ssize_t bytes_read = read(fd_cmd, record.cmd, MAX_CMD_LEN -1);
        if (bytes_read > 0)
        {
            for(ssize_t i = 0; i<bytes_read-1;i++)
            {
                if(record.cmd[i] == '\0')
                {
                    record.cmd[i]=' ';
                }
            }
            record.cmd[bytes_read] = '\0';
        }
        close(fd_cmd);
    }
    write_to_db(db_fd, &record);
}

void snapshot(int db_fd)
{
    DIR *dir=opendir("/proc");
    if (!dir)
    {
        perror("eroare la deschidere /proc\0");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if(entry->d_type == DT_DIR && is_num(entry->d_name))
        {
            proc_parsing(entry->d_name, db_fd);
        }
    }
    closedir(dir);
}

//aceeeasi fct ca la main_fileops_indexer.c

void exit_update(int db_fd)
{
    db_header_t header;
    set_write_lock(db_fd, 0, sizeof(db_header_t));

    lseek(db_fd, 0, SEEK_SET);
    read(db_fd, &header, sizeof(db_header_t));

    header.active_writers--;
    if(header.active_writers == 0)
    {
        header.snapshot_state = STATE_SEALED;
    }

    lseek(db_fd, 0, SEEK_SET);
    write(db_fd, &header, sizeof(db_header_t));
    release_lock(db_fd, 0, sizeof(db_header_t));
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <db_process_path>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *db_path = argv[1];
    int db_fd = open(db_path, O_RDWR | O_CREAT, 0644);
    if (db_fd == -1)
    {
        perror("eroare: baza de date introdusa nu exista");
        return EXIT_FAILURE;
    }

    set_write_lock(db_fd, 0, sizeof(db_header_t));
    
    struct stat db_stat;
    fstat(db_fd, &db_stat);

    if (db_stat.st_size == 0)
    {
        db_header_t initial_header = 
        {
            .magic =DB_MAGIC_NUMBER,
            .format_ver = DB_FORMAT_VERSION,
            .snapshot_id = (uint64_t)time(NULL),
            .snapshot_state = STATE_IN_PROG,
            .active_writers = 0,
            .record_count = 0
        };
        write(db_fd, &initial_header, sizeof(db_header_t));
    }

    db_header_t header;
    lseek(db_fd, 0, SEEK_SET);
    read(db_fd, &header, sizeof(db_header_t));

    header.active_writers++;
    header.snapshot_state = STATE_IN_PROG;

    lseek(db_fd, 0, SEEK_SET);
    write(db_fd, &header, sizeof(db_header_t));
    release_lock(db_fd, 0, sizeof(db_header_t));

    snapshot(db_fd);
    
    exit_update(db_fd);
    close(db_fd);
    return EXIT_SUCCESS;
}