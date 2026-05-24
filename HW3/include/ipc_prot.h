#ifndef IPC_PROT_H
#define IPC_PROT_H

#include <limits.h>
#include <semaphore.h>
#include <stdint.h>
#include <sys/types.h>

#define IPC_MAGIC "INV4"
#define FORMAT_VER 1
#define MAX_JOB_Q_SZ 2048
#define MAX_RES_Q_SZ 4096
#define MAX_WORKERS 64

// rep un director de parcurs
typedef struct __attribute__((packed)) {
  char dir_path[PATH_MAX];
  int depth;
} job_t;

typedef struct __attribute__((packed)) {
  char path[PATH_MAX];
  uid_t uid;
  gid_t gid;
  uint64_t size;
  uint64_t mtime;
  mode_t mode;
  char hash_sha[65];
} file_record_t;

typedef struct __attribute__((packed)) {
  int32_t worker_id;
  pid_t pid;
  int32_t exit_status;
  uint32_t jobs_processed;
  uint32_t files_sent;
  uint64_t bytes_sent;
  uint64_t wall_time_ms;
  uint64_t user_cpu_us;
  uint64_t sys_cpu_us;
} worker_stats_t;

typedef struct {
  job_t jobs[MAX_JOB_Q_SZ];
  uint32_t head; // index de unde se citeste urm job
  uint32_t tail; //--"-- se scrie
  sem_t mutex;   // sem pentru acces head/tail
  sem_t items;   // cate joburi in coada
  sem_t spaces;  // cate locuri libere in coada
} job_queue_t;

// buffer circular pt metadate
typedef struct {
  file_record_t records[MAX_RES_Q_SZ];
  uint32_t head;
  uint32_t tail;
  sem_t mutex;
  sem_t items;  // aici asteapta managerul
  sem_t spaces; // in caz de backpressure workerul asteapta aici
} result_channel_t;

// mmap layout principal

typedef struct {
  char magic[4];
  uint32_t version;
  uint32_t total_wrk;
  uint32_t active_wrk;
  uint32_t terminate_flag;
  uint32_t pending_jobs;
  sem_t glb_mutex; //
  job_queue_t job_queue;
  result_channel_t results;
  worker_stats_t stats[MAX_WORKERS];
} ipc_layout_t;

#endif