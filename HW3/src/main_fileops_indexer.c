#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>


#include "../include/db_format.h"
#include "../include/sync_utils.h"

/*inregistrare in db*/
void write_to_db(int db_fd, file_record_t *record)
{
    db_header_t header;

    set_write_lock(db_fd, 0, sizeof(db_header_t));
    lseek(db_fd, 0, SEEK_SET);
    read(db_fd, &header, sizeof(db_header_t));

    /*calcul inregistrare noua*/
    off_t new_offset = sizeof(db_header_t) + (header.record_count * sizeof(file_record_t));
    
    /*actualizare header*/
    header.record_count++;
    lseek(db_fd, 0, SEEK_SET);
    write(db_fd, &header, sizeof(db_header_t));

    /*deblocare header*/
    release_lock(db_fd, 0, sizeof(db_header_t));

    /*blocam zona inregistrarii si scriem*/
    set_write_lock(db_fd, new_offset, sizeof(file_record_t));

    lseek(db_fd, new_offset, SEEK_SET);
    write(db_fd, record, sizeof(file_record_t));
    release_lock(db_fd, new_offset, sizeof(file_record_t));
}

//parcurgerea directoarelor

void parc_dir (int db_fd, const char *dir_path)
{
    DIR *dir = opendir(dir_path);
    if(!dir)
    {
        perror("directoriul nu exista sau nu are permisiune de deschidere(eroare la opendir)");
        return;
    }

    struct dirent *entry;
    struct stat st;
    char path_full[PATH_MAX];

    while ((entry = readdir(dir)) != NULL)
    {
        //evitam curent si parinte pentru a nu intra in bucla
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        snprintf(path_full, PATH_MAX, "%s%s", dir_path, entry->d_name); //calea absolutaa completa

        if(lstat(path_full, &st) == -1)
        {
            continue;
        }
        file_record_t record;
        memset(&record, 0, sizeof(file_record_t));
        strncpy(record.path, path_full, PATH_MAX -1 );
        record.size = st.st_size;
        record.mtime = st.st_mtime;
        record.st_dev = st.st_dev;
        record.st_ino = st.st_ino;

        if (S_ISREG(st.st_mode)) record.type = FILE_TYPE_RGLR;
        else if (S_ISDIR(st.st_mode)) record.type = FILE_TYPE_DIR;
        else if (S_ISLNK(st.st_mode)) record.type = FILE_TYPE_SYML;
        else record.type = FILE_TYPE_OTH;

        write_to_db(db_fd, &record);

        if(S_ISDIR(st.st_mode))
        {
            parc_dir(db_fd, path_full);
        }
    }
    closedir(dir);    
}

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

    lseek(db_fd, 0, sizeof(db_header_t));
    write(db_fd, &header, sizeof(db_header_t));
    release_lock(db_fd, 0, sizeof(db_header_t));
}
int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "utilizare: %s <cale_dir> <cale_db>\n", argv[0]);
    }

    const char *target_dir = argv[1];
    const char *db_path = argv[2];

    int db_fd = open(db_path, O_RDWR | O_CREAT, 0644);
    if (db_fd == -1)
    {
        perror("eroare la deschiderea bazei de date");
        return EXIT_FAILURE;
    }

    set_write_lock(db_fd, 0, sizeof(db_header_t));

    struct stat db_stat;
    fstat(db_fd, &db_stat);

    if (db_stat.st_size == 0)
    {
        db_header_t initial_header = {
            .magic =DB_MAGIC_NUMBER,
            .format_ver = DB_FORMAT_VERSION,
            .snapshot_id = (uint64_t)time(NULL),
            .snapshot_state = STATE_IN_PROG,
            .active_writers = 0,
            .record_count = 0
        };
        write (db_fd, &initial_header, sizeof(db_header_t));
    }

    db_header_t header;
    lseek(db_fd, 0, SEEK_SET);
    read(db_fd, &header, sizeof(db_header_t));

    header.active_writers++;
    header.snapshot_state = STATE_IN_PROG;

    lseek(db_fd, 0, SEEK_SET);
    write(db_fd, &header, sizeof(db_header_t));
    release_lock(db_fd, 0, sizeof(db_header_t));

    parc_dir(db_fd, target_dir);

    exit_update(db_fd);

    close(db_fd);
    return EXIT_SUCCESS;
}