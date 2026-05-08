/*definirea formatarii ptr baza de date*/
#ifndef DB_FORMAT_H
#define DB_FORMAT_H

#include <linux/limits.h> /*pathmax*/
#include <stdint.h>

#define DB_MAGIC_NUMBER 0x534E4150 /*SNAP in hex*/
#define DB_FORMAT_VERSION 1

/*valori fixe ptr lseek*/
#define MAX_COMM_LEN 16
#define MAX_CMD_LEN 256
#define HASH_LEN 65 /*64 + /0*/

typedef enum { STATE_IN_PROG = 0, STATE_SEALED = 1 } db_state_t;

typedef enum {
  FILE_TYPE_RGLR = 0,
  FILE_TYPE_DIR = 1,
  FILE_TYPE_SYML = 2,
  FILE_TYPE_OTH = 3
} file_type_t;

/*declarate in uint pentru portabilitate intre 64 si 32 biti*/
typedef struct {
  uint32_t magic;
  uint32_t format_ver;
  uint64_t snapshot_id;
  uint8_t snapshot_state;
  uint32_t active_writers;
  uint32_t record_count;
} __attribute__((packed)) db_header_t; /*spune compilatorului sa nu mai puna
                                          padding in memorie, se strica baza*/

typedef struct {
  char path[PATH_MAX];
  uint8_t type;
  uint64_t size;
  uint64_t mtime;
  char hash[HASH_LEN];
  uint64_t st_dev;
  uint64_t st_ino;
} __attribute__((packed)) file_record_t;

typedef struct {
  uint32_t pid;
  uint32_t ppid;
  char state;
  char comm[MAX_COMM_LEN];
  char cmd[MAX_CMD_LEN];
  uint64_t rss;
  uint64_t cpu_time;
} __attribute__((packed)) proc_record_t;

#endif