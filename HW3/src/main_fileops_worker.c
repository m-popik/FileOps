#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <dirent.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>

#include "../include/ipc_prot.h"
void usage_instr(const char *prog_name)
{
    fprintf(stderr, "Usage: %s --worker-id <id> --ipc <path>\n", prog_name);
}

//temporar
void calculate_sha(const char *file_path, char *output_hash)
{
    strncpy(output_hash, "dummy_hash", 64);
    output_hash[64] = '\0';
}

int main(int argc, char *argv[])
{
    int worker_id = -1;
    char *ipc_path = NULL;

    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);

    for (int i=1; i<argc;i++) {
        if (strcmp(argv[i], "--worker-id") == 0 && i+1<argc)
        {
            worker_id = atoi(argv [++i]);
        }else if (strcmp(argv[i], "--ipc") == 0 && i+1 < argc)
        {
            ipc_path = argv[++i];
        }
    }

    if (worker_id == -1 || ipc_path == NULL || worker_id >= MAX_WORKERS)
    {
        usage_instr(argv[0]);
        return EXIT_FAILURE;
    }

    int ipc_fd = open(ipc_path, O_RDWR);
    if (ipc_fd == -1)
    {
        perror("eroare la deschiderea ipc in worker");
        return EXIT_FAILURE;
    }

    struct stat st;
    if (fstat(ipc_fd, &st) == -1)
    {
        perror("fstat error");
        close(ipc_fd);
        return EXIT_FAILURE;
    }
    
    //mapare
    void *ipc_ptr = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, ipc_fd, 0);
    if (ipc_ptr == MAP_FAILED)
    {
        perror("eroare la mmap");
        close(ipc_fd);
        return EXIT_FAILURE;
    }
    close(ipc_fd); //maparea ramane valida

    ipc_layout_t *layout = (ipc_layout_t *)ipc_ptr;

    //initializare locala
    uint32_t local_files_sent = 0;
    uint32_t local_jobs_proc = 0;
    uint64_t local_bytes_sent = 0;

    while (1) {
        //asteptam job
        sem_wait(&layout->job_queue.items);
        //verif conditia de terminare
        sem_wait(&layout->glb_mutex);
        if(layout->terminate_flag == 1)
        {
            sem_post(&layout->glb_mutex);
            break;
        }
        sem_post(&layout->glb_mutex);

        //extragem din ring buffer
        sem_wait(&layout->job_queue.mutex);
        job_t current_job = layout->job_queue.jobs[layout->job_queue.head];
        layout->job_queue.head = (layout->job_queue.head + 1) %MAX_JOB_Q_SZ;
        sem_post(&layout->job_queue.mutex);

        //eliberare in coada de joburi
        sem_post(&layout->job_queue.spaces);
        local_jobs_proc++;

        DIR *dir = opendir(current_job.dir_path);
        if (dir == NULL)
            continue;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL){
            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") ==0)
                continue;

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", current_job.dir_path, entry->d_name);

        struct stat file_st;
        //lstat detecteaza linkurile simbolice
            if(lstat(full_path, &file_st) == -1)
                continue;
        
            if (S_ISLNK(file_st.st_mode))
                continue;
        
            if (S_ISDIR(file_st.st_mode))
            {
                //daca este subdirector il adaugam ca job in coada
                sem_wait(&layout->job_queue.spaces);
                sem_wait(&layout->job_queue.mutex);

                strncpy(layout->job_queue.jobs[layout->job_queue.tail].dir_path, full_path, PATH_MAX);
                layout->job_queue.tail = (layout->job_queue.tail +1) %MAX_JOB_Q_SZ;

                sem_post(&layout->job_queue.mutex);
                sem_post(&layout->job_queue.items);
            }
            else if (S_ISREG(file_st.st_mode))
            {
                //daca este regulat facem un file_record_t
                file_record_t new_record;
                memset(&new_record, 0, sizeof(file_record_t));

                strncpy(new_record.path, full_path, PATH_MAX);
                new_record.uid = file_st.st_uid;
                new_record.gid = file_st.st_gid;
                new_record.size = file_st.st_size;
                new_record.mtime = file_st.st_mtime;
                new_record.mode = file_st.st_mode;
                calculate_sha(full_path, new_record.hash_sha);

                //bagam in canalul de rezultate, daca e plin workerul asteapta
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
}