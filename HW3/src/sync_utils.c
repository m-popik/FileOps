/*am definit prin include o fct ajutatoare ca sa nu mai scriu aceleasi definitii de mai multe ori*/
#include "../include/sync_utils.h"
#include <stdio.h>

static int apply_lock(int fd, int type, off_t offset, off_t length)
{
    struct flock fl;

    fl.l_type = type;
    fl.l_whence = SEEK_SET;
    fl.l_start = offset;
    fl.l_len = length;
    fl.l_pid = getpid();

    if (fcntl(fd, F_SETLKW, &fl) == -1)
    {
        perror("eroare la fcntl");
        return -1;
    }
    return 0;
}

int set_write_lock(int fd, off_t offset, off_t length)
{
    return apply_lock(fd, F_WRLCK, offset, length);
}

int set_read_lock(int fd, off_t offset, off_t length)
{
    return apply_lock(fd, F_RDLCK, offset, length);
}

int release_lock(int fd, off_t offset, off_t length)
{
    return apply_lock(fd, F_UNLCK, offset, length);
}