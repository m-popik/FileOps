#ifndef SYNC_UTILS_H
#define SYNC_UTILS_H

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

int set_write_lock(int fd, off_t offset, off_t length);

int set_read_lock(int fd, off_t offset, off_t length);

int release_lock(int fd, off_t offset, off_t length);
#endif