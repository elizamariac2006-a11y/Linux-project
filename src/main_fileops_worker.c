#define _POSIX_C_SOURCE 200809L 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <getopt.h>
#include <dirent.h>
#include <openssl/sha.h>
#include <sys/resource.h>
#include <time.h>
#include <signal.h>
#include <limits.h> 
#include "../include/header_t4.h"

///flag global si handler pentru semnale 
volatile sig_atomic_t shutdown_requested = 0;

void handle_sigterm(int sig) 
{
    (void)sig; // evit warning-ul de parametru neutilizat
    shutdown_requested = 1;
}

// functie pentru trimiterea mesajelor atomice prin pipe
void trimite_mesaj_control(int control_fd, const char *type, int worker_id, const char *extra) 
{
    if (control_fd == -1) return;

    char buffer[PIPE_BUF];
    int n = snprintf(buffer, sizeof(buffer), "T5MSG type=%s worker_id=%d %s\n", type, worker_id, extra ? extra : "");

    if (n > 0 && n < (int)sizeof(buffer)) 
    {
        ssize_t w = write(control_fd, buffer, strlen(buffer));
        (void)w; // evit warning
    }
}

int calculeaza_adancime(const char *radacina, const char *cale) 
{
    int count = 0;
    const char *p = cale + strlen(radacina);
    while (*p) {
        if (*p == '/') count++;
        p++;
    }
    return count;
}

void calculeaza_sha256(const char *drum, unsigned char *output) 
{
    int fd = open(drum, O_RDONLY);
    if (fd == -1) 
        return;

    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    unsigned char buffer[4096];
    ssize_t bytes;

    while ((bytes = read(fd, buffer, sizeof(buffer))) > 0) 
    { 
        SHA256_Update(&ctx, buffer, bytes);
    }

    SHA256_Final(output, &ctx); 
    close(fd);
}

int main(int argc, char *argv[]) 
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start); 

    int worker_id = -1;
    char *ipc_path = NULL;
    char *cale_radacina = NULL;
    int adancime_max = -1;
    int simulate_ms = 0;
    int control_fd = -1; 

    /// Dictionar de mapare 
    struct option optiuni_comanda[] = 
    { 
        {"worker-id", required_argument, 0, 'w'},  
        {"ipc", required_argument, 0, 'i'}, 
        {"root", required_argument, 0, 'r'},
        {"max-depth", required_argument, 0, 'd'},
        {"simulate-work-ms", required_argument, 0, 's'},
        {"control-fd", required_argument, 0, 'c'}, 
        {0, 0, 0, 0}  
    };

    /// Parsare argumente linia de comanda
    int opt;
    while ((opt = getopt_long(argc, argv, "w:i:r:d:s:c:", optiuni_comanda, NULL)) != -1) 
    {
        if (opt == 's') {
            simulate_ms = atoi(optarg);
        }
        else if (opt == 'w') 
            worker_id = atoi(optarg); 
        else if (opt == 'i') 
            ipc_path = optarg;       
        else if (opt == 'r') 
            cale_radacina = optarg;
        else if (opt == 'd')
            adancime_max = atoi(optarg);
        else if (opt == 'c') 
            control_fd = atoi(optarg);
    }

    if (worker_id == -1 || ipc_path == NULL) {
        fprintf(stderr, "Eroare: Lipsesc argumentele obligatorii pt workeri\n");
        exit(EXIT_FAILURE);
    }

    //inregistrare handler
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigterm;
    sigaction(SIGTERM, &sa, NULL);

    int fd = open(ipc_path, O_RDWR);
    if(fd == -1) 
        exit(EXIT_FAILURE);

    mapped_data *date_partajate = (mapped_data *)mmap(NULL, sizeof(mapped_data), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (date_partajate == MAP_FAILED) 
        exit(EXIT_FAILURE);
    
    close(fd);


    while (!shutdown_requested) 
    {
        char cale[MAX_PATH];
        
        /// Verific daca am un job 
        if (sem_wait(&date_partajate->queue.items_available_cnt) == -1) 
        {
            // Daca sem_wait a fost intrerupt de semnal verific flag-ul de shutdown
            if (shutdown_requested) break;
            continue;
        }

        sem_wait(&date_partajate->queue.sem_tail_head);

        ///extrag calea directorului care trebuie scanat
        strncpy(cale, date_partajate->queue.paths[date_partajate->queue.head], MAX_PATH);
        date_partajate->queue.head = (date_partajate->queue.head + 1) % MAX_JOBS;

        ///vad daca am mesaj special de STOP de la manager 
        if (strcmp(cale, "STOP") == 0) 
        {
            sem_post(&date_partajate->queue.sem_tail_head);
            break;
        }
        
        sem_post(&date_partajate->queue.sem_tail_head);
        sem_post(&date_partajate->queue.free_slots_cnt);
        
        ///simulez intarziere artificiala 
        if(simulate_ms > 0)
        {
            usleep(simulate_ms * 1000);
        }

        DIR *dir = opendir(cale);
        if (dir == NULL) 
        {
            ///raportez eroarea pe pipe
            char err_buf[64];
            snprintf(err_buf, sizeof(err_buf), "error=opendir_failed path=%s", cale);
            trimite_mesaj_control(control_fd, "ERROR", worker_id, err_buf);

            sem_wait(&date_partajate->counter_mutex);
            date_partajate->active_jobs--;
            sem_post(&date_partajate->counter_mutex);
            continue;
        }

        struct dirent *e;
        while ((e = readdir(dir)) != NULL) 
        {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) 
                continue;

            char noua_cale[MAX_PATH];
            snprintf(noua_cale, MAX_PATH, "%s/%s", cale, e->d_name);

            struct stat st;
            if (lstat(noua_cale, &st) == -1)  
                continue;
            if (S_ISLNK(st.st_mode)) 
                continue; 

            if (S_ISDIR(st.st_mode)) 
            {
                int ad_noua = calculeaza_adancime(cale_radacina, noua_cale);
                
                if (adancime_max == -1 || ad_noua <= adancime_max) 
                {
                    sem_wait(&date_partajate->counter_mutex);
                    date_partajate->active_jobs++;
                    sem_post(&date_partajate->counter_mutex);
                    
                    sem_wait(&date_partajate->queue.free_slots_cnt);
                    sem_wait(&date_partajate->queue.sem_tail_head);
                  
                    strncpy(date_partajate->queue.paths[date_partajate->queue.tail], noua_cale, MAX_PATH);
                    date_partajate->queue.tail = (date_partajate->queue.tail + 1) % MAX_JOBS;
                    
                    sem_post(&date_partajate->queue.sem_tail_head);
                    sem_post(&date_partajate->queue.items_available_cnt);
                }
            } 
            else if (S_ISREG(st.st_mode)) 
            {
                unsigned char hash_temporar[32];
                memset(hash_temporar, 0, 32);
                calculeaza_sha256(noua_cale, hash_temporar);

                sem_wait(&date_partajate->free_slots_in_results);
                sem_wait(&date_partajate->results_semaphor);

                file_record *rec = &date_partajate->results[date_partajate->res_tail];
                strncpy(rec->path, noua_cale, MAX_PATH);
                rec->size = (uint64_t)st.st_size; 
                rec->mtime = (uint64_t)st.st_mtime;
                rec->mode = st.st_mode;
                rec->uid = st.st_uid;
                rec->gid = st.st_gid;
                memcpy(rec->sha256, hash_temporar, 32);
                
                date_partajate->res_tail = (date_partajate->res_tail + 1) % MAX_RESULTS;
                date_partajate->stats[worker_id].files_emitted++;
                date_partajate->stats[worker_id].bytes_emitted += st.st_size;
                
                sem_post(&date_partajate->results_semaphor);
                sem_post(&date_partajate->items_avail_in_results);
            }
        }
        closedir(dir);
      
        sem_wait(&date_partajate->counter_mutex);
        date_partajate->active_jobs--;
        date_partajate->stats[worker_id].jobs_processed++;
        sem_post(&date_partajate->counter_mutex);

        // raportez job_done
        char stats_buf[128];
        snprintf(stats_buf, sizeof(stats_buf), "jobs=%d files=%d bytes=%llu", 
                 date_partajate->stats[worker_id].jobs_processed,
                 date_partajate->stats[worker_id].files_emitted,
                 (unsigned long long)date_partajate->stats[worker_id].bytes_emitted);
        trimite_mesaj_control(control_fd, "JOB_DONE", worker_id, stats_buf);
    }

    // notificare de iesire din functie in functie de starea in care s a iesit
    if (shutdown_requested) 
    {
        trimite_mesaj_control(control_fd, "WORKER_EXITING", worker_id, "reason=shutdown");
    } 
    else 
    {
        trimite_mesaj_control(control_fd, "WORKER_EXITING", worker_id, "reason=normal");
    }

    // xuratare pipe control
    if (control_fd != -1) 
    {
        close(control_fd);
    }

    struct rusage u;
    getrusage(RUSAGE_SELF, &u);
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    long long ms = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;

    date_partajate->stats[worker_id].wall_time_ms = ms;
    date_partajate->stats[worker_id].user_cpu_us = u.ru_utime.tv_sec * 1000000 + u.ru_utime.tv_usec;
    date_partajate->stats[worker_id].sys_cpu_us = u.ru_stime.tv_sec * 1000000 + u.ru_stime.tv_usec;
    date_partajate->stats[worker_id].pid = getpid();
    date_partajate->stats[worker_id].worker_id = worker_id;

    munmap(date_partajate, sizeof(mapped_data));
    return 0;
}
