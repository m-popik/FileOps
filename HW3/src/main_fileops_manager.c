#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <dirent.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../include/ipc_prot.h"

typedef struct {
  file_record_t *records;
  uint32_t count;
  uint32_t capacity;

} record_collector_t;

void howto(const char *prog) {
  fprintf(stderr, "Ghid:");
  fprintf(stderr, "Inventariere: %s --root <dir> --workers <N>\n", prog);
  fprintf(stderr, "Verificare: %s <db_path> --verify\n", prog);
  fprintf(stderr, "Sumar: %s <db_path> --db\n", prog);
}
ipc_layout_t *init_ipc(const char *ipc_path, const char *root_dir,
                       int num_workers) {
  int fd = open(ipc_path, O_CREAT | O_RDWR | O_TRUNC, 0666);
  if (fd == -1) {
    perror("eroare creare ipc");
    exit(EXIT_FAILURE);
  }

  if (ftruncate(fd, sizeof(ipc_layout_t)) == -1) {
    perror("eroare ftruncate");
    exit(EXIT_FAILURE);
  }

  ipc_layout_t *layout = mmap(NULL, sizeof(ipc_layout_t),
                              PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (layout == MAP_FAILED) {
    perror("eroare la mmap manager");
    exit(EXIT_FAILURE);
  }
  close(fd);

  memset(layout, 0, sizeof(ipc_layout_t));
  memcpy(layout->magic, IPC_MAGIC, 4);
  layout->version = FORMAT_VER;
  layout->total_wrk = num_workers;
  layout->active_wrk = num_workers;
  layout->terminate_flag = 0;
  layout->pending_jobs = 1;

  sem_init(&layout->glb_mutex, 1, 1);

  sem_init(&layout->job_queue.mutex, 1, 1);
  sem_init(&layout->job_queue.items, 1, 1); // root
  sem_init(&layout->job_queue.spaces, 1, MAX_JOB_Q_SZ);

  sem_init(&layout->results.mutex, 1, 1);
  sem_init(&layout->results.items, 1, 0);
  sem_init(&layout->results.spaces, 1, MAX_RES_Q_SZ);

  strncpy(layout->job_queue.jobs[0].dir_path, root_dir, PATH_MAX);
  layout->job_queue.jobs[0].depth = 0;
  layout->job_queue.head = 0;
  layout->job_queue.tail = 1;

  return layout;
}

// scrierea atomica a bazei de date

void write_db_atomic(const char *db_path, ipc_layout_t *layout,
                     record_collector_t *collection) {
  char tmp_path[PATH_MAX];
  snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", db_path);
  int fd = open(tmp_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (fd == -1) {
    perror("eroare creare db temporar");
    return;
  }

  // header
  uint32_t complete_flag = 1;
  write(fd, IPC_MAGIC, 4);
  write(fd, &layout->version, sizeof(uint32_t));
  write(fd, &complete_flag, sizeof(uint32_t));
  write(fd, &collection->count, sizeof(uint32_t));
  write(fd, &layout->total_wrk, sizeof(uint32_t));

  // file records
  if (collection->count > 0) {
    write(fd, collection->records, sizeof(file_record_t) * collection->count);
  }

  write(fd, layout->stats, sizeof(worker_stats_t) * layout->total_wrk);

  if (rename(tmp_path, db_path) == -1) {
    perror("eroare la rename");
  }
}

// header db asa cum este scris de write_db_atomic (20 bytes)
typedef struct __attribute__((packed)) {
  char magic[4];
  uint32_t version;
  uint32_t complete;
  uint32_t file_record_count;
  uint32_t worker_count;
} db_header_t;

int verify_db(const char *db_path) {
  int fd = open(db_path, O_RDONLY);
  if (fd == -1) {
    fprintf(stderr, "eroare verify: nu pot deschide '%s'\n", db_path);
    return EXIT_FAILURE;
  }

  struct stat st;
  if (fstat(fd, &st) == -1) {
    perror("fstat error");
    close(fd);
    return EXIT_FAILURE;
  }

  db_header_t hdr;
  if (read(fd, &hdr, sizeof(hdr)) != (ssize_t)sizeof(hdr)) {
    fprintf(stderr, "eroare verify: header incomplet in '%s'\n", db_path);
    close(fd);
    return EXIT_FAILURE;
  }
  close(fd);

  if (memcmp(hdr.magic, "INV4", 4) != 0) {
    fprintf(stderr, "eroare verify: magic invalid (asteptat INV4)\n");
    return EXIT_FAILURE;
  }
  if (hdr.version != FORMAT_VER) {
    fprintf(stderr, "eroare verify: versiune invalida (%u)\n", hdr.version);
    return EXIT_FAILURE;
  }

  off_t expected = (off_t)sizeof(db_header_t) +
                   (off_t)sizeof(file_record_t) * hdr.file_record_count +
                   (off_t)sizeof(worker_stats_t) * hdr.worker_count;
  if (st.st_size != expected) {
    fprintf(stderr,
            "eroare verify: dimensiune incorecta (asteptat %ld, gasit %ld)\n",
            (long)expected, (long)st.st_size);
    return EXIT_FAILURE;
  }
  printf("%s este valid\n", db_path);
  return EXIT_SUCCESS;
}

int dump_db(const char *db_path) {
  int fd = open(db_path, O_RDONLY);
  if (fd == -1) {
    fprintf(stderr, "eroare dump: nu pot deschide '%s'\n", db_path);
    return EXIT_FAILURE;
  }

  db_header_t hdr;
  if (read(fd, &hdr, sizeof(hdr)) != (ssize_t)sizeof(hdr)) {
    fprintf(stderr, "eroare dump: header incomplet in '%s'\n", db_path);
    close(fd);
    return EXIT_FAILURE;
  }
  close(fd);

  printf("magic=%.4s\n", hdr.magic);
  printf("version=%u\n", hdr.version);
  printf("complete=%u\n", hdr.complete);
  printf("file_record_count=%u\n", hdr.file_record_count);
  printf("worker_count=%u\n", hdr.worker_count);
  return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
  char *root_dir = NULL;
  char *ipc_path = "data/ipc.mmap";
  int num_workers = 0;
  char *db_path = "data/inventory.db";
  int do_verify = 0;
  int do_dump = 0;
  int max_depth = -1;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
      root_dir = argv[++i];
    } else if (strcmp(argv[i], "--workers") == 0 && i + 1 < argc) {
      num_workers = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--ipc") == 0 && i + 1 < argc) {
      ipc_path = argv[++i];
    } else if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
      db_path = argv[++i];
    } else if (strcmp(argv[i], "--verify") == 0) {
      do_verify = 1;
    } else if (strcmp(argv[i], "--dump") == 0) {
      do_dump = 1;
    } else if (strcmp(argv[i], "--max-depth") == 0 && i + 1 < argc) {
      max_depth = atoi(argv[++i]);
    }
  }

  if (do_verify)
    return verify_db(db_path);
  if (do_dump)
    return dump_db(db_path);

  if (!root_dir || num_workers < 1) {
    howto(argv[0]);
    return EXIT_FAILURE;
  }

  ipc_layout_t *layout = init_ipc(ipc_path, root_dir, num_workers);

  record_collector_t collection = {NULL, 0, 1000};
  collection.records = malloc(sizeof(file_record_t) * collection.capacity);

  for (int i = 0; i < num_workers; i++) {
    pid_t pid = fork();
    if (pid == 0) {
      char worker_id_str[16];
      snprintf(worker_id_str, sizeof(worker_id_str), "%d", i);
      
      char max_depth_str[16];
      snprintf(max_depth_str, sizeof(max_depth_str), "%d", max_depth);

      // path, arg0, arg1,...
      execl("./bin/fileops_worker", "./bin/fileops_worker", "--worker-id",
            worker_id_str, "--ipc", ipc_path, "--max-depth", max_depth_str, NULL);

      perror("eroare la exec worker");
      exit(EXIT_FAILURE);
    }
    layout->stats[i].pid = pid;
    layout->stats[i].worker_id = i;
  }

  int workers_running = num_workers;
  while (workers_running > 0) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 100000000; // +100ms
    if (ts.tv_nsec >= 1000000000) {
      ts.tv_sec += 1;
      ts.tv_nsec -= 1000000000;
    }

    if (sem_timedwait(&layout->results.items, &ts) == 0) {
      sem_wait(&layout->results.mutex);
      file_record_t rec = layout->results.records[layout->results.head];
      layout->results.head = (layout->results.head + 1) % MAX_RES_Q_SZ;
      sem_post(&layout->results.mutex);
      sem_post(&layout->results.spaces);

      if (collection.count >= collection.capacity) {
        collection.capacity *= 2;
        collection.records = realloc(
            collection.records, sizeof(file_record_t) * collection.capacity);
      }
      collection.records[collection.count++] = rec;
    }

    int status;
    pid_t p = waitpid(-1, &status, WNOHANG);
    if (p > 0) {
      workers_running--;
      for (int i = 0; i < num_workers; i++) {
        if (layout->stats[i].pid == p) {
          layout->stats[i].exit_status = WEXITSTATUS(status);
          break;
        }
      }
    }
  }

  while (sem_trywait(&layout->results.items) == 0) {
    sem_wait(&layout->results.mutex);
    file_record_t rec = layout->results.records[layout->results.head];
    layout->results.head = (layout->results.head + 1) % MAX_RES_Q_SZ;
    sem_post(&layout->results.mutex);
    sem_post(&layout->results.spaces);

    if (collection.count >= collection.capacity) {
      collection.capacity *= 2;
      collection.records = realloc(collection.records,
                                   sizeof(file_record_t) * collection.capacity);
    }
    collection.records[collection.count++] = rec;
  }

  write_db_atomic(db_path, layout, &collection);
  free(collection.records);
  munmap(layout, sizeof(ipc_layout_t));

  return EXIT_SUCCESS;
}