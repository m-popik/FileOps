#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <dirent.h>
#include <fcntl.h>
#include <openssl/evp.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "../include/ipc_prot.h"

volatile sig_atomic_t worker_got_sigterm = 0;

void handle_worker_sigterm(int sig) {
  worker_got_sigterm = 1;
}

void usage_instr(const char *prog_name) {
  fprintf(stderr, "Usage: %s --worker-id <id> --ipc <path>\n", prog_name);
}

void calculate_sha(const char *file_path, char *output_hash) {
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hash_len;
  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();

  if (mdctx == NULL) {
    strncpy(output_hash, "CTX error", 65);
    return;
  }

  if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1) {
    EVP_MD_CTX_free(mdctx);
    strncpy(output_hash, "EVP_init error", 65);
    return;
  }

  FILE *file = fopen(file_path, "rb");
  if (!file) {
    EVP_MD_CTX_free(mdctx);
    strncpy(output_hash, "file_unreadable", 65);
    return;
  }

  unsigned char buffer[32768];
  int bytesRead = 0;

  while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
    EVP_DigestUpdate(mdctx, buffer, bytesRead);
  }

  EVP_DigestFinal_ex(mdctx, hash, &hash_len);

  fclose(file);
  EVP_MD_CTX_free(mdctx);

  static const char hex[] = "0123456789abcdef";
  for (unsigned int i = 0; i < hash_len; i++) {
    output_hash[i * 2] = hex[hash[i] >> 4];
    output_hash[i * 2 + 1] = hex[hash[i] & 0x0f];
  }
  output_hash[hash_len * 2] = '\0';
}

void send_control_msg(int fd, const char *type, int worker_id,
                      const char *extra) {
  if (fd < 0)
    return;
  char buf[256];
  int len = snprintf(buf, sizeof(buf), "T5MSG type=%s worker_id=%d %s\n", type,
                     worker_id, extra);
  write(fd, buf, len);
}

int main(int argc, char *argv[]) {
  int worker_id = -1;
  char *ipc_path = NULL;
  int max_depth = -1;
  unsigned long long mil = 1000000;
  int control_fd = -1;
  int sim_time_ms = 0;

  struct timeval start_time, end_time;
  gettimeofday(&start_time, NULL);

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--worker-id") == 0 && i + 1 < argc) {
      worker_id = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--ipc") == 0 && i + 1 < argc) {
      ipc_path = argv[++i];
    } else if (strcmp(argv[i], "--max-depth") == 0 && i + 1 < argc) {
      max_depth = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--control-fd") == 0 && i + 1 < argc) {
      control_fd = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--simulate-work-ms") == 0 && i + 1 < argc) {
      sim_time_ms = atoi(argv[++i]);
    }
  }

  if (worker_id == -1 || ipc_path == NULL || worker_id >= MAX_WORKERS) {
    usage_instr(argv[0]);
    return EXIT_FAILURE;
  }

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handle_worker_sigterm;
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);

  int ipc_fd = open(ipc_path, O_RDWR);
  if (ipc_fd == -1) {
    perror("eroare la deschiderea ipc in worker");
    return EXIT_FAILURE;
  }

  struct stat st;
  if (fstat(ipc_fd, &st) == -1) {
    perror("fstat error");
    close(ipc_fd);
    return EXIT_FAILURE;
  }

  // mapare
  void *ipc_ptr =
      mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, ipc_fd, 0);
  if (ipc_ptr == MAP_FAILED) {
    perror("eroare la mmap");
    close(ipc_fd);
    return EXIT_FAILURE;
  }
  close(ipc_fd); // maparea ramane valida

  ipc_layout_t *layout = (ipc_layout_t *)ipc_ptr;

  // initializare locala
  uint32_t local_files_sent = 0;
  uint32_t local_jobs_proc = 0;
  uint64_t local_bytes_sent = 0;

  while (1) {
    // asteapta un job sau semnalul de terminare
    if (sem_wait(&layout->job_queue.items) == -1) {
      if (errno == EINTR && worker_got_sigterm)
        break;
      continue;
    }

    // verifica flag-ul de terminare
    sem_wait(&layout->glb_mutex);
    int should_stop = layout->terminate_flag;
    sem_post(&layout->glb_mutex);

    if (should_stop || worker_got_sigterm) {
      // trimitem si altui worker semnalul de terminare
      sem_post(&layout->job_queue.items);
      break;
    }

    // extragem din ring buffer
    sem_wait(&layout->job_queue.mutex);
    job_t current_job = layout->job_queue.jobs[layout->job_queue.head];
    layout->job_queue.head = (layout->job_queue.head + 1) % MAX_JOB_Q_SZ;
    sem_post(&layout->job_queue.mutex);
    sem_post(&layout->job_queue.spaces);
    local_jobs_proc++;

    if (sim_time_ms > 0) {
      usleep(sim_time_ms * 1000);
    }

    DIR *dir = opendir(current_job.dir_path);
    if (dir != NULL) {
      struct dirent *entry;
      while ((entry = readdir(dir)) != NULL) {
        if (worker_got_sigterm)
          break;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
          continue;

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", current_job.dir_path,
                 entry->d_name);

        struct stat file_st;
        // lstat detecteaza linkurile simbolice
        if (lstat(full_path, &file_st) == -1)
          continue;

        if (S_ISLNK(file_st.st_mode))
          continue;

        if (S_ISDIR(file_st.st_mode)) {
          if (max_depth == -1 || current_job.depth < max_depth) {
            sem_wait(&layout->glb_mutex);
            layout->pending_jobs++;
            sem_post(&layout->glb_mutex);

            sem_wait(&layout->job_queue.spaces);
            sem_wait(&layout->job_queue.mutex);
            strncpy(layout->job_queue.jobs[layout->job_queue.tail].dir_path,
                    full_path, PATH_MAX);
            layout->job_queue.jobs[layout->job_queue.tail].depth =
                current_job.depth + 1;
            layout->job_queue.tail =
                (layout->job_queue.tail + 1) % MAX_JOB_Q_SZ;
            sem_post(&layout->job_queue.mutex);
            sem_post(&layout->job_queue.items);
          }
        } else if (S_ISREG(file_st.st_mode)) {
          // daca este regulat facem un file_record_t
          file_record_t new_record;
          memset(&new_record, 0, sizeof(file_record_t));

          strncpy(new_record.path, full_path, PATH_MAX);
          new_record.uid = file_st.st_uid;
          new_record.gid = file_st.st_gid;
          new_record.size = file_st.st_size;
          new_record.mtime = file_st.st_mtime;
          new_record.mode = file_st.st_mode;
          calculate_sha(full_path, new_record.hash_sha);

          sem_wait(&layout->results.spaces);
          sem_wait(&layout->results.mutex);

          layout->results.records[layout->results.tail] = new_record;
          layout->results.tail = (layout->results.tail + 1) % MAX_RES_Q_SZ;

          sem_post(&layout->results.mutex);
          sem_post(&layout->results.items);

          local_files_sent++;
          local_bytes_sent += file_st.st_size;
        }
      }
      closedir(dir);
    }

    send_control_msg(control_fd, "JOB_DONE", worker_id, "done");

    sem_wait(&layout->glb_mutex);
    layout->pending_jobs--;
    if (layout->pending_jobs == 0) {
      layout->terminate_flag = 1;
      for (uint32_t i = 0; i < layout->total_wrk; i++) {
        sem_post(&layout->job_queue.items);
      }
    }
    sem_post(&layout->glb_mutex);
  }

  send_control_msg(control_fd, "WORKER_EXITING", worker_id, "reason=shutdown");
  if (control_fd >= 0) close(control_fd);

  gettimeofday(&end_time, NULL);
  uint64_t wall_time = (end_time.tv_sec - start_time.tv_sec) * 1000 +
                       (end_time.tv_usec - start_time.tv_usec) / 1000;

  struct rusage usage;
  uint64_t user_time = 0, sys_time = 0;

  if (getrusage(RUSAGE_SELF, &usage) == 0) {
    user_time = usage.ru_utime.tv_sec * mil + usage.ru_utime.tv_usec;
    sys_time = usage.ru_stime.tv_sec * mil + usage.ru_stime.tv_usec;
  }

  layout->stats[worker_id].jobs_processed = local_jobs_proc;
  layout->stats[worker_id].files_sent = local_files_sent;
  layout->stats[worker_id].bytes_sent = local_bytes_sent;
  layout->stats[worker_id].wall_time_ms = wall_time;
  layout->stats[worker_id].user_cpu_us = user_time;
  layout->stats[worker_id].sys_cpu_us = sys_time;

  if (munmap(ipc_ptr, st.st_size) == -1) {
    perror("eroare la unmap");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}